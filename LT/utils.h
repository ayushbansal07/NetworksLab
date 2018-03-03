#define BUFF_SIZE 1024
#define MAX_FILE_NAME 30
#define MAX_SEQ_NO 65535
#define WINDOW_SIZE 3

struct segment{
	int seqNo;
	int len;
	char data[BUFF_SIZE];
};

struct file_details{
    char filename[MAX_FILE_NAME];
    int filesize;
    int no_of_chunks;
};

