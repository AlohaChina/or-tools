************************************************************************
file with basedata            : me48_.bas
initial value random generator: 711222092
************************************************************************
projects                      :  1
jobs (incl. supersource/sink ):  22
horizon                       :  158
RESOURCES
  - renewable                 :  2   R
  - nonrenewable              :  0   N
  - doubly constrained        :  0   D
************************************************************************
PROJECT INFORMATION:
pronr.  #jobs rel.date duedate tardcost  MPM-Time
    1     20      0       18       19       18
************************************************************************
PRECEDENCE RELATIONS:
jobnr.    #modes  #successors   successors
   1        1          3           2   3   4
   2        3          3           5   6   8
   3        3          3           7   9  10
   4        3          3           6   7  12
   5        3          2          11  21
   6        3          3          15  16  21
   7        3          2          14  20
   8        3          3          14  15  18
   9        3          1          18
  10        3          3          11  14  16
  11        3          1          20
  12        3          1          13
  13        3          3          17  18  19
  14        3          1          19
  15        3          1          17
  16        3          2          17  19
  17        3          1          20
  18        3          1          21
  19        3          1          22
  20        3          1          22
  21        3          1          22
  22        1          0        
************************************************************************
REQUESTS/DURATIONS:
jobnr. mode duration  R 1  R 2
------------------------------------------------------------------------
  1      1     0       0    0
  2      1     2       5    9
         2     9       2    8
         3     9       4    6
  3      1     8       3    6
         2     8       2    8
         3     9       2    4
  4      1     2       9   10
         2     7       9    7
         3    10       9    3
  5      1     1       9    7
         2     2       9    5
         3     3       9    4
  6      1     1       9    5
         2     3       8    4
         3     6       7    3
  7      1     4       9   10
         2     5       9    9
         3     9       8    9
  8      1     3       6    3
         2     4       4    3
         3     8       3    2
  9      1     3       5    9
         2     8       4    6
         3    10       1    5
 10      1     1       5    7
         2     1       4    9
         3     7       4    1
 11      1     8       6    7
         2     8       7    6
         3    10       4    3
 12      1     1       8    6
         2     8       5    4
         3     9       2    3
 13      1     1       6    5
         2     2       6    4
         3     4       5    3
 14      1     1       7   10
         2     5       6   10
         3    10       6    9
 15      1     7       6   10
         2     8       5    7
         3     9       3    5
 16      1     3       8    7
         2     9       7    7
         3    10       6    7
 17      1     4       9    7
         2     6       5    3
         3     6       6    2
 18      1     2       9    9
         2     7       8    6
         3     8       8    5
 19      1     3       8    5
         2     3       5    7
         3     7       3    3
 20      1     1       7   10
         2     1       8    9
         3     7       7    6
 21      1     5       5    7
         2     6       5    6
         3     7       4    6
 22      1     0       0    0
************************************************************************
RESOURCEAVAILABILITIES:
  R 1  R 2
   39   43
************************************************************************
