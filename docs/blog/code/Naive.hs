-- docs/blog/code/Naive.hs
-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
--
-- The naive, direct Haskell models of the Free monad and the Cofree
-- comonad -- the data types the C++ library (free.hpp / cofree.hpp)
-- transcribes almost line for line. Compiled with GHC 9.6.7; the Driver
-- module exercises the examples.

module Naive where

-- 3de91951-9177-4ee5-bc19-642e7c567079
-- Free f a: a Pure value of type a, or one F-layer of further Free
-- computations. This is exactly smd::fixpoint::Free<F, A>'s
-- std::variant<A, F<Free<F,A>>>.
data Free f a
  = Pure a
  | Free (f (Free f a))

instance Functor f => Functor (Free f) where
  fmap g (Pure a)  = Pure (g a)
  fmap g (Free fa) = Free (fmap (fmap g) fa)

instance Functor f => Applicative (Free f) where
  pure = Pure
  Pure g  <*> x = fmap g x
  Free fg <*> x = Free (fmap (<*> x) fg)

instance Functor f => Monad (Free f) where
  Pure a  >>= k = k a
  Free fa >>= k = Free (fmap (>>= k) fa)
-- 3de91951-9177-4ee5-bc19-642e7c567079 end

-- 1dd9eb1e-81e1-467f-a2ce-1c9822e645cf
-- Cofree f a: an F-shaped tree where every node carries an annotation of
-- type a at its head and one F-layer of annotated children as its tail.
-- This is exactly smd::fixpoint::Cofree<F, A>'s { A head; F<Cofree<F,A>> }.
data Cofree f a = a :< f (Cofree f a)
infixr 5 :<

instance Functor f => Functor (Cofree f) where
  fmap g (a :< fa) = g a :< fmap (fmap g) fa

-- A base-only Comonad class, so these modules need no external packages.
class Functor w => Comonad w where
  extract   :: w a -> a
  duplicate :: w a -> w (w a)
  duplicate = extend id
  extend    :: (w a -> b) -> w a -> w b
  extend f = fmap f . duplicate
  {-# MINIMAL extract, (duplicate | extend) #-}

instance Functor f => Comonad (Cofree f) where
  extract (a :< _)          = a
  duplicate w@(_ :< fa)     = w :< fmap duplicate fa
-- 1dd9eb1e-81e1-467f-a2ce-1c9822e645cf end

-- Tiny examples. Cofree Maybe is a non-empty list; Free Maybe counts steps.

exampleNel :: Cofree Maybe Int
exampleNel = 1 :< Just (2 :< Just (3 :< Nothing))

sumNel :: Num a => Cofree Maybe a -> a
sumNel (a :< Nothing) = a
sumNel (a :< Just t)  = a + sumNel t

-- extract reads the head; duplicate re-annotates each node with its subtree.
headsOfSuffixes :: Cofree Maybe a -> Cofree Maybe a
headsOfSuffixes = fmap extract . duplicate
