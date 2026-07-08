-- docs/blog/code/Church.hs
-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
--
-- The Church / Boehm-Berarducci encoding of the Free monad: represent a
-- Free value by its own fold (catamorphism). Compiled with GHC 9.6.7.

{-# LANGUAGE RankNTypes #-}

module Church where

import Naive (Free (..))

-- 11f5813d-9505-4bcf-8ef9-bf9bdc526c6b
-- A Free value *is* the function that folds it: give it what to do with a
-- Pure leaf (a -> r) and an F-algebra (f r -> r) and it hands back the r.
-- Note there is no (Functor f) anywhere in this type or its Monad instance
-- -- the fmap that naive Free needs is pushed into the algebra `kf`.
newtype CFree f a = CFree { runCFree :: forall r. (a -> r) -> (f r -> r) -> r }

instance Functor (CFree f) where
  fmap g (CFree c) = CFree (\kp kf -> c (kp . g) kf)

instance Applicative (CFree f) where
  pure a = CFree (\kp _ -> kp a)
  CFree cf <*> CFree cx = CFree (\kp kf -> cf (\g -> cx (kp . g) kf) kf)

instance Monad (CFree f) where
  CFree c >>= k = CFree (\kp kf -> c (\a -> runCFree (k a) kp kf) kf)

-- Lifting a single layer is the one place a Functor is still needed -- but
-- only to build a value, never to sequence (>>=). That asymmetry is the
-- whole idea Freer pushes on (see Freer.hs).
liftF :: Functor f => f a -> CFree f a
liftF fa = CFree (\kp kf -> kf (fmap kp fa))
-- 11f5813d-9505-4bcf-8ef9-bf9bdc526c6b end

-- 2e91a89f-a8ff-439a-bf14-ae2b86ebd61c
-- The encoding is isomorphic to naive Free (for a Functor f): fromNaive
-- runs the fold that rebuilds the CPS form; toNaive instantiates the
-- answer type r at Free f a itself, with Pure and Free as the two folders.
fromNaive :: Functor f => Free f a -> CFree f a
fromNaive m = CFree (\kp kf ->
  let go (Pure a)  = kp a
      go (Free fa) = kf (fmap go fa)
  in go m)

toNaive :: CFree f a -> Free f a
toNaive (CFree c) = c Pure Free
-- 2e91a89f-a8ff-439a-bf14-ae2b86ebd61c end
