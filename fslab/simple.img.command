# open and close a file
o1 FILE.TXT
c1

# read the file containing the GPL
o2 GPL.TXT
r2 100000
c2

# read a file in a directory
o3 SIMPLE.DIR/READ.ME
r3 100
c3

# create a hello world file
n4 NEW.TXT
#o4 NEW.TXT
w4 Hello world!
c4

# read the newly created file
o5 NEW.TXT
r5 1000
w5 foobar
c5
