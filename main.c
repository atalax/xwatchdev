/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Josef Gajdusek
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * */

#include <errno.h>
#include <dirent.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include <X11/extensions/XInput2.h>

void print_usage()
{
	printf(
		"usage: xwatchdev [-f] [-v]\n"
		"optional arguments:\n"
		"  -f, --fork		fork on start\n"
		"  -v, --verbose	verbose mode\n"
		"  -h, --help		display this help\n");
}

char *scriptdir = NULL;
bool verbose = false;

int filter(const struct dirent *de)
{
	return de->d_type == DT_REG;
}

void run_scripts()
{
	struct dirent **des;

	int ret = scandir(scriptdir, &des, filter, alphasort);

	for (int i = 0; i < ret; i++) {
		char path[256];
		struct stat st;
		snprintf(path, sizeof(path), "%s/%s", scriptdir, des[i]->d_name);
		stat(path, &st);
		if (!(st.st_mode & S_IXUSR)) {
			if (verbose)
				fprintf(stderr, "Tried to execute %s but it is not executable\n", path);
			continue;
		}
		pid_t cid = fork();
		if (cid == -1) {
			fprintf(stderr, "Failed to fork when executing %s\n", path);
			continue;
		} else if(cid == 0) { // Child
			execl(path, des[i]->d_name, NULL); // Does not return if successful
			fprintf(stderr, "Failed to execute %s with error code = %d\n", path, errno);
			exit(EXIT_FAILURE);
		}
		waitpid(cid, NULL, 0);
		printf("Executed %s\n", path);
	}

}

void process_event(XIHierarchyEvent *xhe)
{
	const char *str;
	if (xhe->flags & XISlaveAdded)
		str = "added";
	else
		str = "removed";
	setenv("ACTION", str, true);

	if (xhe->info->use == XISlavePointer)
		str = "pointer";
	else if (xhe->info->use == XISlaveKeyboard)
		str = "keyboard";
	setenv("TYPE", str, true);
	run_scripts();
}

struct option longopts[] = {
	{"--fork",		no_argument,	0,		'f'},
	{"--verbose",	no_argument,	0,		'v'},
	{0,				0,				0,		 0 }
};

int main(int argc, char *argv[])
{
	int c;
	bool shouldfork = false;

	while (true) {
		int optind = 0;

		c = getopt_long(argc, argv, "fvh", longopts, &optind);
		if (c == -1)
			break;

		switch(c) {
		case 'f':
		{
			shouldfork = true;
		}
		case 'v':
			verbose = true;
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
		}
	}

	if (shouldfork) {
		pid_t pid = fork();
		if (pid == -1) {
			fprintf(stderr, "Failed to fork with error code = %d", errno);
			exit(EXIT_FAILURE);
		} else if (pid != 0) { // Parent
			exit(EXIT_SUCCESS);
		}
		if (verbose)
			printf("Forked\n");
	}

	scriptdir = malloc(256);
	snprintf(scriptdir, 256, "%s/.xwatchdev", getenv("HOME"));
	if (verbose)
		printf("The script directory is %s\n", scriptdir);

	// Thanks to http://who-t.blogspot.cz/2009/06/xi2-recipies-part-2.html
	Display *dis = XOpenDisplay(NULL); // Open root
	unsigned char mask[2] = {0, 0};
	XISetMask(mask, XI_HierarchyChanged);
	XIEventMask ximask = {XIAllDevices, sizeof(mask), mask};
	XISelectEvents(dis, DefaultRootWindow(dis), &ximask, 1);

	if (verbose)
		printf("Entering event loop\n");

	while (true) {
		XEvent ev;
		XNextEvent(dis, &ev);
		XGetEventData(dis, &ev.xcookie);
		XIHierarchyEvent *xhe = ev.xcookie.data;
		if (xhe->flags & (XISlaveAdded | XISlaveRemoved))
			process_event(xhe);
		XFreeEventData(dis, &ev.xcookie);
	}
}
