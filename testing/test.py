
import sys
import subprocess
import threading

testProjects = ['./gerbv_example/dan', \
                    './gerbv_example/am-test', \
                    './gerbv_example/eaglecad1', \
                    './gerbv_example/jj']

class Builder(threading.Thread):
    def __init__(self, project_dir, output_dir_name ):
        threading.Thread.__init__(self)
        self._project_dir = project_dir
        self._output_dir_name = output_dir_name

    def run(self):
        build_project( self._project_dir, self._output_dir_name )

def build_project( project_dir, output_dir_name ):
    prog_path = subprocess.check_output('pwd')[0:-1] + '/../pcb2gcode' 
    subprocess.Popen( args=prog_path, cwd=project_dir ).wait()
    subprocess.Popen( args='mkdir -p '+output_dir_name+' && mv *.ngc '+output_dir_name, \
                          cwd=project_dir, shell=True ).wait()

def check_project( project_dir ):
    ret = subprocess.call(['diff', '-q', project_dir+'/old', project_dir+'/new'])
    if ret != 0:
        print 'ERROR: differences found in test project ' + project_dir
        subprocess.call(['meld', project_dir+'/old', project_dir+'/new'])
    else:
        print 'No differences found.'

def clean_project( project_dir ):
    subprocess.call(['rm', '-rf', project_dir+'/old', project_dir+'/new', project_dir+'/*.png'])


def build_projects( output_dir_name ):
    builders = []
    for project in testProjects:
        print 'Building ' + project
        builder = Builder( project, output_dir_name )
        builders.append(builder)
        builder.start()
    
    for builder in builders:
        builder.join()

def check_projects():
    for project in testProjects:
        print 'Checking ' + project
        check_project( project )

def clean_projects():
    for project in testProjects:
        print 'Cleaning up.'
        clean_project( project )

if sys.argv[1] == 'buildold':
    build_projects('old')
elif sys.argv[1] == 'buildnew':
    build_projects('new')
elif sys.argv[1] == 'cmp':
    check_projects()
elif sys.argv[1] == 'clean':
    clean_projects()
