-- docs/blog/code/Cofreer.hs
-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
--
-- Is there a CoFreer? Yes -- the exact dual of Freer = Free (Coyoneda f)
-- is Cofreer = Cofree (Coyoneda f), a cofree comonad that needs no Functor
-- instance for its signature. Compiled with GHC 9.6.7.

{-# LANGUAGE RankNTypes #-}

module Cofreer where

import Naive (Cofree (..), Comonad (..))
import Freer (Coyoneda (..))

-- 783faca1-9ed1-4b2b-8cf2-98d4241c07b2
-- Cofreer f = Cofree (Coyoneda f). Because Coyoneda f is a Functor for
-- *every* f, Naive's `instance Functor f => Comonad (Cofree f)` fires with
-- no constraint on f. The proof is that these comonad operations have no
-- `Functor f` in their signatures and still typecheck:
type Cofreer f = Cofree (Coyoneda f)

cofreerExtract :: Cofreer f a -> a
cofreerExtract = extract

cofreerDuplicate :: Cofreer f a -> Cofreer f (Cofreer f a)
cofreerDuplicate = duplicate

-- Coyoneda's lift needs no Functor (you build the comonad for free);
-- its lower does (you only pay a Functor to interpret one layer out).
liftCoyoneda :: f a -> Coyoneda f a
liftCoyoneda = Coyoneda id

lowerCoyoneda :: Functor f => Coyoneda f a -> f a
lowerCoyoneda (Coyoneda h fx) = fmap h fx
-- 783faca1-9ed1-4b2b-8cf2-98d4241c07b2 end

-- 4e76f426-2ebe-4fab-8b78-3d3098132802
-- The variance-dual choice. Yoneda is the other free-functor completion --
-- the forall-dual of Coyoneda's exists -- and Cofree (Yoneda f) is *also*
-- a Comonad for every f. The asymmetry that decides which to use:
--   liftCoyoneda  :: f a -> Coyoneda f a          -- free to *build*
--   lowerCoyoneda :: Functor f => Coyoneda f a -> f a
--   liftYoneda    :: Functor f => f a -> Yoneda f a
--   lowerYoneda   :: Yoneda f a -> f a            -- free to *observe*
-- Coyoneda pairs with Free/Freer (structures you construct); Yoneda pairs
-- with Cofree (structures you observe out of).
newtype Yoneda f a = Yoneda { runYoneda :: forall b. (a -> b) -> f b }

instance Functor (Yoneda f) where
  fmap g (Yoneda y) = Yoneda (\k -> y (k . g))

liftYoneda :: Functor f => f a -> Yoneda f a
liftYoneda fa = Yoneda (\k -> fmap k fa)

lowerYoneda :: Yoneda f a -> f a
lowerYoneda (Yoneda y) = y id

type CofreerY f = Cofree (Yoneda f)
-- 4e76f426-2ebe-4fab-8b78-3d3098132802 end

-- An example: a Cofreer Maybe Int (an annotated non-empty list), built
-- with liftCoyoneda and consumed with the constraint-free comonad ops.
exampleCofreer :: Cofreer Maybe Int
exampleCofreer =
  10 :< liftCoyoneda (Just (20 :< liftCoyoneda (Just (30 :< liftCoyoneda Nothing))))

cofreerToList :: Cofreer Maybe a -> [a]
cofreerToList (a :< co) = a : case lowerCoyoneda co of
  Nothing -> []
  Just t  -> cofreerToList t
