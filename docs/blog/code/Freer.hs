-- docs/blog/code/Freer.hs
-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
--
-- The Freer monad of Kiselyov & Ishii, "Freer Monads, More Extensible
-- Effects" (Haskell Symposium 2015): a free monad that needs no Functor
-- instance for its signature. Compiled with GHC 9.6.7.

{-# LANGUAGE GADTs #-}

module Freer where

import Naive (Free (..))

-- 2ecd9399-edf9-442e-991a-e84ce397c528
-- Freer f a: a Pure value, or a command `f x` paired with a continuation
-- (x -> Freer f a). The continuation -- not a nested functor -- is what
-- carries the rest of the computation, so *no `Functor f` is ever needed*.
data Freer f a where
  RPure   :: a -> Freer f a
  RImpure :: f x -> (x -> Freer f a) -> Freer f a

instance Functor (Freer f) where
  fmap g (RPure a)      = RPure (g a)
  fmap g (RImpure fx k) = RImpure fx (fmap g . k)

instance Applicative (Freer f) where
  pure = RPure
  RPure g      <*> x = fmap g x
  RImpure fx k <*> x = RImpure fx ((<*> x) . k)

instance Monad (Freer f) where
  RPure a      >>= k2 = k2 a
  RImpure fx k >>= k2 = RImpure fx ((>>= k2) . k)

-- lift a single command into the monad -- again, no Functor constraint.
liftFreer :: f a -> Freer f a
liftFreer c = RImpure c RPure
-- 2ecd9399-edf9-442e-991a-e84ce397c528 end

-- 938a4593-7bec-43e9-a426-1f04c391ab29
-- Coyoneda f -- the free functor on any f. It is a Functor for *every* f,
-- with no constraint, because fmap just accumulates in the (x -> a) slot.
data Coyoneda f a where
  Coyoneda :: (x -> a) -> f x -> Coyoneda f a

instance Functor (Coyoneda f) where
  fmap g (Coyoneda h fx) = Coyoneda (g . h) fx

-- Freer is exactly Free (Coyoneda f): the continuation in RImpure is the
-- (x -> a) of a Coyoneda, and Free supplies the recursion. These two
-- witnesses compile, so the isomorphism is real, not a slogan.
toFree :: Freer f a -> Free (Coyoneda f) a
toFree (RPure a)      = Pure a
toFree (RImpure fx k) = Free (Coyoneda (toFree . k) fx)

fromFree :: Free (Coyoneda f) a -> Freer f a
fromFree (Pure a)                = RPure a
fromFree (Free (Coyoneda h fx)) = RImpure fx (fromFree . h)
-- 938a4593-7bec-43e9-a426-1f04c391ab29 end

-- ddc83df5-7c70-4f63-b8db-8c544466cb56
-- An effect signature with NO Functor instance (its result type is an
-- index, not a covariant slot). Freer still gives a monad over it.
data Console a where
  PutLine :: String -> Console ()
  GetLine :: Console String

putLine :: String -> Freer Console ()
putLine s = liftFreer (PutLine s)

getLine_ :: Freer Console String
getLine_ = liftFreer GetLine

greet :: Freer Console ()
greet = do
  putLine "what is your name?"
  name <- getLine_
  putLine ("hello, " ++ name)

-- A pure handler: thread a list of inputs, accumulate outputs.
runConsole :: [String] -> Freer Console a -> ([String], a)
runConsole _        (RPure a)              = ([], a)
runConsole ins      (RImpure cmd k) = case cmd of
  PutLine s -> let (out, a) = runConsole ins (k ())
               in (s : out, a)
  GetLine   -> case ins of
    (i : rest) -> runConsole rest (k i)
    []         -> runConsole [] (k "")
-- ddc83df5-7c70-4f63-b8db-8c544466cb56 end
