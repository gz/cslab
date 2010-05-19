from subprocess import Popen, PIPE, STDOUT
import time

filename = 'matrix.c'
tags = ('// <config>', '// </config>')

# Read source code
f = open(filename, 'r')
template = f.read()
f.close()

begin_config = template.find(tags[0]) + len(tags[0])
end_config = template.find(tags[1])

if begin_config == -1 or end_config == -1 or end_config <= begin_config:
    print 'Invalid template'
    exit()

configs = [
    ('basic', """
#define NUM_THREADS 1
    """),
    
    ('2threads', """
#define ENABLE_PARALLELIZATION
#define NUM_THREADS 2
""")
]

markers = ('Decomposition time:', 'Checking time:', 'Overall time:')

basic = None

for name, config in configs:
    print '-- %s --' % name
    content = template[0:begin_config] + '\n' + config + '\n' + template[end_config:]

    # Store changed file
    f = open(filename, 'w')
    f.write(content)
    f.close()

    p = Popen('make && ./parallel.exe 1024', shell=True, stdin=PIPE, stdout=PIPE, stderr=STDOUT, close_fds=True)
    output = p.stdout.read()
    
    data = {}
    for line in output.split('\n'):
        line = line.strip()
        if line.startswith(markers):
            i = line.find(':')
            title = line[:i]
            line = line[i+1:]
            xs = line.rsplit(',', 3)
            data[title] = {}
            for x in xs:
                y = x.strip().split(' ')
                data[title][y[1]] = float(y[0][:-1])

    if basic is None:
        basic = data
                
    for title in data.iterkeys():
        print title
        for x, y in data[title].iteritems():
            if x != 'elapsed':
                continue
            print '\t%s=%s' % (x, basic[title][x] / y)
    
    time.sleep(0.25)