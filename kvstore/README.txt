/**********************************************************************/
/* Key-value Store Manual
 * @version: 05/02/2017 */
/**********************************************************************/

# [ATTENTION] without -n, the node with load latest snapshot and replay its own log

# Accepted argv:
-a         (author)
-v         (verbose mode, recommended)
-p: #port  (default: 4711)
-i: P/S    ("identity", P for primary, S for secondary)
-n         ("new" implies that this is the very first time that this machine
             serves as a the storage node, else start data recovery) 


# Basic operation examples:
1. put abc,fn,hello world
2. get abc,fn
3. cput abc,fn,hello world,good day!
4. dele abc,fn
5. getlist abc,email (or getlist abc,mail)
(* binary value will be better)
