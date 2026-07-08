-- docs/blog/code/Driver.hs
-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
--
-- Exercises every construction so the excerpts are not merely well typed
-- but actually run. Build/run:  runghc Driver.hs   (or ghc --make Driver.hs)

module Main (main) where

import Naive   (sumNel, exampleNel, headsOfSuffixes, extract)
import FixRel  (Fix, NatF, two, toNat,
                freeToFix, fixToFree, fixToCofree, cofreeToFix)
import Church  (toNaive, fromNaive)
import Freer   (greet, runConsole)
import Cofreer (exampleCofreer, cofreerToList, cofreerExtract)

check :: String -> Bool -> IO ()
check label ok = putStrLn ((if ok then "ok   " else "FAIL ") ++ label)

-- Structural equality on Fix NatF, so the round-trips can be asserted.
eqFixNat :: Fix NatF -> Fix NatF -> Bool
eqFixNat a b = toNat a == toNat b

main :: IO ()
main = do
  check "Naive: sum of non-empty list"      (sumNel exampleNel == 6)
  check "Naive: extract . duplicate = id"   (sumNel (headsOfSuffixes exampleNel) == 6)
  check "Naive: extract head"               (extract exampleNel == 1)

  check "FixRel: Fix NatF value"            (toNat two == 2)
  check "FixRel: Fix -> Free -> Fix"        (eqFixNat (freeToFix (fixToFree two)) two)
  check "FixRel: Fix -> Cofree -> Fix"      (eqFixNat (cofreeToFix (fixToCofree two)) two)

  -- Church <-> naive round trip, observed through the naive fold.
  let churched = toNaive (fromNaive (fixToFree two))
  check "Church: toNaive . fromNaive = id"  (eqFixNat (freeToFix churched) two)

  let (out, ()) = runConsole ["Ada"] greet
  check "Freer: console handler output"     (out == ["what is your name?", "hello, Ada"])

  check "Cofreer: extract (no Functor f)"   (cofreerExtract exampleCofreer == 10)
  check "Cofreer: toList"                    (cofreerToList exampleCofreer == [10, 20, 30])
