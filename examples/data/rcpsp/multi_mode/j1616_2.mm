************************************************************************
file with basedata            : md208_.bas
initial value random generator: 1853990534
************************************************************************
projects                      :  1
jobs (incl. supersource/sink ):  18
horizon                       :  142
RESOURCES
  - renewable                 :  2   R
  - nonrenewable              :  2   N
  - doubly constrained        :  0   D
************************************************************************
PROJECT INFORMATION:
pronr.  #jobs rel.date duedate tardcost  MPM-Time
    1     16      0       14       12       14
************************************************************************
PRECEDENCE RELATIONS:
jobnr.    #modes  #successors   successors
   1        1          3           2   3   4
   2        3          3           6   8   9
   3        3          3           6   7   9
   4        3          2           5   8
   5        3          2          12  13
   6        3          3          10  12  17
   7        3          3           8  10  11
   8        3          3          13  16  17
   9        3          1          13
  10        3          1          14
  11        3          2          12  17
  12        3          2          14  16
  13        3          1          15
  14        3          1          15
  15        3          1          18
  16        3          1          18
  17        3          1          18
  18        1          0        
************************************************************************
REQUESTS/DURATIONS:
jobnr. mode duration  R 1  R 2  N 1  N 2
------------------------------------------------------------------------
  1      1     0       0    0    0    0
  2      1     3       6    7    8    0
         2     3       9    6    8    0
         3    10       2    5    0    4
  3      1     1       3    6    5    0
         2     7       3    3    0    5
         3     8       3    1    0    4
  4      1     4       8    6    0    9
         2     4       9    7    6    0
         3    10       5    4    0   10
  5      1     7       6    6    9    0
         2     8       5    5    0    2
         3     8       5    5    6    0
  6      1     2       4    8    0    6
         2     8       1    7    8    0
         3     8       4    7    0    4
  7      1     2       6    5    3    0
         2     7       5    4    0    6
         3     8       3    4    0    3
  8      1     2       6   10    0   10
         2     7       5    9    9    0
         3     9       5    9    0    6
  9      1     2       9    9    0    6
         2     4       6    7    0    6
         3     8       6    6    0    6
 10      1     3       7    6    9    0
         2     5       7    5    6    0
         3     9       7    5    0   10
 11      1     4       7    6    0    5
         2     8       5    5    9    0
         3    10       4    4    5    0
 12      1     1       5    7    7    0
         2     8       5    7    0    6
         3    10       5    6    5    0
 13      1     2       7    3    8    0
         2     3       7    3    0    7
         3     7       6    3    0    5
 14      1     1       8    2    0    4
         2     5       8    2    0    2
         3     8       5    2    6    0
 15      1     1       8    3    0    5
         2     8       4    2    0    4
         3    10       2    2    0    2
 16      1     2       7    7    9    0
         2    10       3    3    0    8
         3    10       3    6    9    0
 17      1     3       9    8    4    0
         2     4       6    6    4    0
         3     9       2    2    0    1
 18      1     0       0    0    0    0
************************************************************************
RESOURCEAVAILABILITIES:
  R 1  R 2  N 1  N 2
   32   39   50   52
************************************************************************
