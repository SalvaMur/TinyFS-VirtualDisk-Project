
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static int do_copyout( const char *tfsfile, const char *filename );

int main( int argc, char *argv[] )
{
	char line[1024];
	char cmd[1024];
	char arg1[1024];
	char arg2[1024];
	int inumber, result, args;

	if(argc!=2) {
		printf("use: %s <diskfile> \n",argv[0]);
		return 1;
	}

	if(!disk_init(argv[1],2048)) { // 8M: always 2048 blocks
		printf("couldn't initialize %s: %s\n",argv[1],strerror(errno));
		return 1;
	}

	while(1) {
		printf("TinyFS> ");
		fflush(stdout);

		if(!fgets(line,sizeof(line),stdin)) break;

		if(line[0]=='\n') continue;
		line[strlen(line)-1] = 0;

		args = sscanf(line,"%s %s %s",cmd,arg1,arg2);
		if(args==0) continue;

		if(!strcmp(cmd,"debug")) {
			if(args==1) {
				tfs_debug();
			} else {
				printf("use: debug\n");
			}
		} else if(!strcmp(cmd,"getsize")) {
			if(args==2) {
				result = tfs_getsize(arg1);
				if(result>=0) {
					printf("file %s has size %d\n",arg1,result);
				} else {
					printf("getsize failed!\n");
				}
			} else {
				printf("use: getsize filename\n");
			}
			
		} else if(!strcmp(cmd,"delete")) {
			if(args==2) {
				if((inumber=tfs_delete(arg1))) {
					printf("file %s inode %d deleted.\n",arg1,inumber);
				} else {
					printf("delete failed!\n");	
				}
			} else {
				printf("use: delete filename\n");
			}
		} else if(!strcmp(cmd,"cat")) {
			if(args==2) {
				if(!do_copyout(arg1,"/dev/stdout")) {
					printf("cat failed!\n");
				}
			} else {
				printf("use: cat filename\n");
			}

		} else if(!strcmp(cmd,"copyout")) {
			if(args==3) {
				if((inumber=do_copyout(arg1,arg2))) {
					printf("copied file %s (inode %d) to file %s\n",arg1,inumber,arg2);
				} else {
					printf("copy failed!\n");
				}
			} else {
				printf("use: copyout <src_tfsfile> <dstfile>\n");
			}

		} else if(!strcmp(cmd,"help")) {
			printf("Commands are:\n");
			printf("    debug\n");
			printf("    delete  filename\n");
			printf("    getsize  filename\n");
			printf("    cat     filename\n");
			printf("    copyout <src_tfsfile> <dstfile>\n");
			printf("    help\n");
			printf("    quit\n");
			printf("    exit\n");
		} else if(!strcmp(cmd,"quit")) {
			break;
		} else if(!strcmp(cmd,"exit")) {
			break;
		} else {
			printf("unknown command: %s\n",cmd);
			printf("type 'help' for a list of commands.\n");
			result = 1;
		}
	}

	printf("closing emulated disk.\n");
	disk_close();

	return 0;
}

static int do_copyout( const char *tfsfile, const char *filename )
{
	FILE *file;
	int offset=0, result;
	char buffer[16384];

	int inumber;

	file = fopen(filename,"w");
	if(!file) {
		printf("couldn't open %s: %s\n",filename,strerror(errno));
		return 0;
	}

	inumber = tfs_get_inumber(tfsfile);
	if(!inumber) {
		printf("file %s not found \n",tfsfile);
		return 0;
	}
	while(1) {
		result = tfs_read(inumber,buffer,sizeof(buffer),offset);
		if(result<=0) break;
		fwrite(buffer,1,result,file);
		offset += result;
	}

	fclose(file);

	printf("\n%d bytes copied\n",offset);

	return inumber;
}

