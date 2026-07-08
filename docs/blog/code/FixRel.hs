-- docs/blog/code/FixRel.hs
-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
--
-- Fix is the degenerate case of both Free and Cofree:
--   * Free f Void  ~=  Fix f   (a Free with no variables/leaves)
--   * Cofree f ()  ~=  Fix f   (a Cofree with no annotations)
-- These are honest, total isomorphisms, compiled with GHC 9.6.7.

module FixRel where

import Data.Void (Void, absurd)
import Naive     (Free (..), Cofree (..))

-- 406b8975-6c11-4ee3-bdd1-fc8d2b5d37ad
-- The plain fixpoint -- the Haskell twin of C++ smd::fixpoint::Fix<F>.
newtype Fix f = Fix (f (Fix f))

-- Free f a is the least fixpoint of  X |-> a + f X : Fix with variables of
-- type a at the leaves. Kill the variables (a = Void) and Free collapses
-- to Fix. `absurd` witnesses that a Pure leaf can never occur.
freeToFix :: Functor f => Free f Void -> Fix f
freeToFix (Pure v)  = absurd v
freeToFix (Free fa) = Fix (fmap freeToFix fa)

fixToFree :: Functor f => Fix f -> Free f Void
fixToFree (Fix fa) = Free (fmap fixToFree fa)

-- Cofree f a is the greatest fixpoint of  X |-> a * f X : Fix with an
-- annotation of type a at every node. Make the annotation trivial (a = ())
-- and Cofree collapses to Fix.
cofreeToFix :: Functor f => Cofree f () -> Fix f
cofreeToFix (() :< fa) = Fix (fmap cofreeToFix fa)

fixToCofree :: Functor f => Fix f -> Cofree f ()
fixToCofree (Fix fa) = () :< fmap fixToCofree fa
-- 406b8975-6c11-4ee3-bdd1-fc8d2b5d37ad end

-- A witness value: the Peano number 2 as Fix, round-tripped both ways.
-- (NatF as a base-only functor.)
data NatF r = Z | S r
instance Functor NatF where
  fmap _ Z     = Z
  fmap g (S r) = S (g r)

two :: Fix NatF
two = Fix (S (Fix (S (Fix Z))))

toNat :: Fix NatF -> Int
toNat (Fix Z)     = 0
toNat (Fix (S r)) = 1 + toNat r
