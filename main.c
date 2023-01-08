#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>


#define MAX 512 //user's input is less than 512 bytes
int hist_count = 0; //global variable for MyHistory function
int alias_count = 0;  //global variable for MyAlias function
pid_t ppid; //gloabal parent id
pid_t cpid; //global child id

void InteractiveMode();
void BatchMode(char *file);

int ParseCommands(char *userInput); //e.g., "ls -a -l; who; date;" is converted to "ls -al" "who" "date"
int ParseArgs(char *full_line, char *args[]); //e.g., "ls -a -l" is converted to "ls" "-a" "-l"
void ExecuteCommands(char *command, char *full_line);

void MyCD(char *dir_input, int arg_count);
void MyExit();
void MyPath(char *args[], int arg_count);
void MyHistory(char *args[], int arg_count);

void CommandRedirect(char *args[], char *first_command, int arg_count, char *full_line);
void PipeCommands(char *args[], char *first_command, int arg_count);
void signalHandle(int sig);
void MyAlias(char *args[], int arg_count);
void io_redirect(char *command, char *full_line);
void ResetPath();

char CURRENT_DIRECTORY[MAX]; //current directory
char *COMMANDS[MAX]; //commands to be executed
char *MYHISTORY[MAX]; //shell command history
char *MYPATH; //my PATH variable
char *MYALIAS[MAX]; //alias variable
const char *ORIG_PATH_VAR; //The original PATH contents
char *prompt;

int EXIT_CALLED = 0;//Functions seem to treat this as a global variable -DM


int main(int argc, char *argv[]){
    //error checking on user's input
	if (!(argc < 3)) {
		fprintf(stderr, "Error: Too many parameters\n");
		fprintf(stderr, "Usage: './output [filepath]'\n");
		exit(0);//No memory needs to be cleared
	}

    //initialize your shell's enviroment
    MYPATH = (char*) malloc(10240);
	memset(MYPATH, '\0', sizeof(MYPATH));
	ORIG_PATH_VAR = getenv("PATH"); // needs to include <stdlib.h>

    //save the original PATH, which is recovered on exit
	strcpy(MYPATH, ORIG_PATH_VAR);

    //make my own PATH, namely MYPATH
	setenv("MYPATH", MYPATH, 1);

	//gets the parent id and sets it to ppid
    ppid = getpid();

    //handles the signal (Ctrl + C)
    signal(SIGINT, signalHandle);

    //handles the signal (Ctrl + Z)
    signal(SIGTSTP, signalHandle);

	if(argc == 1) InteractiveMode();
     else if(argc == 2) BatchMode(argv[1]);

    //free all variables initialized by malloc()
	free(MYPATH);

	ResetPath();

	return 0;
}

void BatchMode(char *file){
	FILE *fptr = fopen(file, "r");
    //error checking for fopen function
    if(fptr == NULL) {
		fprintf(stderr, "Error: Batch file not found or cannot be opened\n");
		MyExit();
    }

    char *batch_command_line = (char *)malloc(MAX);
    memset(batch_command_line, '\0', sizeof(batch_command_line));

    //reads a line from fptr, stores it into batch_command_line
    while(fgets(batch_command_line, MAX, fptr)){
	//remove trailing newline
	batch_command_line[strcspn(batch_command_line, "\n")] = 0;
	printf("Processing batchfile input: %s\n", batch_command_line);

        //parse batch_command_line to set the array COMMANDS[]
        //for example: COMMANDS[0]="ls -a -l", COMMANDS[1]="who", COMMANDS[2]="date"
        int cmd_count = ParseCommands(batch_command_line);

        //execute commands one by one
        for(int i=0; i< cmd_count; i++){
            char *temp = strdup(COMMANDS[i]); //for example: ls -a -l
            temp = strtok(temp, " "); //get the command
            ExecuteCommands(temp, COMMANDS[i]);
            //free temp
			free(temp);
        }
    }
    //free batch_command_line, and close fptr
	free(batch_command_line);
	fclose(fptr);
}

int ParseCommands(char *str){
	int i = 0;

	char *token = strtok(str, ";"); //breaks str into a series of tokens using ;

	while(token != NULL){
		//error checking for possible bad user inputs
		//Removes Spaces at beginning
		while (token[0] == ' ') {
			int size = strlen(token);
			for (int j=0; j<size; j++) {
				token[j] = token[j+1];
			}
		}

		//If after, removing all whitespaces we're left with a NULL char,
		//then the command is empty and will be ignored
		if (token[0] == '\0') {
			token = strtok(NULL, ";");
			continue;
		}

		//Removes all but one whitespace in between args
		for (int j=0; j<strlen(token); j++) {
			//fprintf(stderr,"Token Edit: %s\n", token);
			if (token[j] == ' ' && token[j+1] == ' ') {
				int size = strlen(token);
				for (int k=j; k<size; k++)
					token[k] = token[k+1];
				j--;
			}
		}

        //save the current token into COMMANDS[]
        COMMANDS[i] = token;
		//printf("%s\n", COMMANDS[i]);
        i++;
        //move to the next token
        token = strtok(NULL, ";");
	}

	return i;
}

void ExecuteCommands(char *command, char *full_line){
	char *args[MAX]; //hold arguments
	//printf("%s\n", full_line);
	MYHISTORY[hist_count%20] = strdup(full_line); //array of commands
	hist_count++;

    //save backup full_line
    char *backup_line = strdup(full_line);

    if (strcmp(command, "alias") == 0 && strchr(full_line, '=') != NULL) {
		//break full_line into a series of tokens by the delimiter space (or " ")
		char *token = strchr(full_line, ' ');
		while (token[0] == ' ') {
			int size = strlen(token);
			for (int j=0; j<size; j++) {
				token[j] = token[j+1];
			}
		}
		MYALIAS[alias_count] = strdup(token);
		alias_count++;
	}
	else {
		//parse full_line to get arguments and save them to args[] array
		int arg_count = ParseArgs(full_line, args);

		//restores full_line
        strcpy(full_line, backup_line);
        free(backup_line);

		//check if built-in function is called
		if(strcmp(command, "cd") == 0)
			MyCD(args[0], arg_count);
		else if(strcmp(command, "exit") == 0)
			EXIT_CALLED = 1;
		else if(strcmp(command, "path") == 0)
			MyPath(args, arg_count);
		else if(strcmp(command, "myhistory") == 0)
			MyHistory(args, arg_count);
		else if(strcmp(command, "alias") == 0)
			MyAlias(args, arg_count);
		else
			CommandRedirect(args, command, arg_count, full_line);
		//free memory used in ParsedArgs() function
		for(int i=0; i<arg_count-1; i++){
			if(args[i] != NULL){
				free(args[i]);
				args[i] = NULL;
			}
		}
	}
}

int ParseArgs(char *full_line, char *args[]){
	int count = 0;

    //break full_line into a series of tokens by the delimiter space (or " ")
	char *token = strtok(full_line, " ");
	//skip over to the first argument
	token = strtok(NULL, " ");

    while(token != NULL){
        //copy the current argument to args[] array
        args[count] = strdup(token);
        count++;
        //move to the next token (or argument)
        token = strtok(NULL, " ");
    }

    return count + 1;
}

void CommandRedirect(char *args[], char *first_command, int arg_count, char *full_line){
	int status;

	//if full_line contains pipelining and redirection, error displayed
	if (strchr(full_line, '|') != NULL && (strchr(full_line, '<') != NULL || strchr(full_line, '>') != NULL)) {
	    fprintf(stderr,"Command cannot contain both pipelining and redirection\n");
	}
	//if full_line contains "<" or ">", then io_redirect() is called
	else if (strchr(full_line, '<') != NULL || strchr(full_line, '>') != NULL) {
		io_redirect(first_command, full_line);
	}
	//if full_line contains "|", then PipeCommands() is called
	else if (strchr(full_line, '|') != NULL) {
		PipeCommands(args, first_command, arg_count);
	}
	else {//else excute the current command
		//set the new cmd[] array so that cmd[0] hold the actual command
		//cmd[1] - cmd[arg_count] hold the actual arguments
		//cmd[arg_count+1] hold the "NULL"
		char *cmd[arg_count + 1];
		cmd[0] = first_command;
		for (int i=1; i<arg_count; i++)
			cmd[i] = args[i-1];
		cmd[arg_count] = '\0';

		cpid = fork();
		if(cpid == 0) {
			pid_t pid = getpid();
			setpgid(pid, 0);
			execvp(*cmd, cmd);
			fprintf(stderr,"%s: command not found\n", *cmd);
			MyExit();//Ensures child exits after executing command
		}
		else {
			waitpid(cpid, &status, WSTOPPED);
			cpid = 0;
		}
	}
}

void InteractiveMode(){
	int status = 0;

    //get custom prompt
    prompt = (char*)malloc(MAX);
    printf("Enter custom prompt: ");
    fgets(prompt, MAX, stdin);

    //remove newline from prompt
    if (prompt[strlen(prompt)-1] == '\n') {
        prompt[strlen(prompt)-1] = '\0';
    }

	while(1){
		char *str = (char*)malloc(MAX);

		printf("%s> ", prompt);
		fgets(str, MAX, stdin);

		//error checking for empty commandline
		if (strlen(str) == 1) {
			continue;
		}

		//remove newline from str
		if (str[strlen(str)-1] == '\n') {
			str[strlen(str)-1] = '\0';
		}

		//parse commands
		int cmd_num = ParseCommands(str);//this function can be better designed

		//execute commands that are saved in COMMANDS[] array
		for(int i=0; i < cmd_num; i++){
			char *temp = strdup(COMMANDS[i]);
			temp = strtok(temp, " ");
			ExecuteCommands(temp, COMMANDS[i]);
			//free temp
			free(temp);
		}

		//ctrl-d kill
		free(str);

		// if exit was selected
		if(EXIT_CALLED) {
		    free(prompt);
		    MyExit();
			break;
		}
	}
}

void MyCD(char *dir_input, int arg_count){
	
}

// printf free malloc (IGNORE: For highlighting puposes)
void MyExit()
{ 
	//Chech to see if there is previous processes
	if (hist_count > 20) 
	{
		hist_count = 20; 
	}
	//Finish all processes
	for(int i=0; i<hist_count; i++) 
	{
		if(MYHISTORY[i] != NULL)
		{
			free(MYHISTORY[i]);
			MYHISTORY[i] = NULL; 
		}
	}

	//Deallocate the path
    setenv("MYPATH", ORIG_PATH_VAR, 1);
    free(MYPATH);
    
	//Exit from the shell
	exit(0);
}

void ResetPath(){
	setenv("PATH", ORIG_PATH_VAR, 1);
}

void AddPath(char *path){
	strcat(MYPATH, ":");
	strcat(MYPATH, path);
	setenv("PATH", MYPATH, 1);
}

void RemovePath(char *path){
	char *nextPath = strtok(MYPATH, ":");
	char *newPath = (char*) malloc(10240);
	int first = 1;
	while (nextPath != NULL)
	{
		if (strcmp(nextPath, path) != 0)
		{
			if (first == 1)
			{
				first = 0;
			} else 
			{
				strcat(newPath, ":");
			}
			strcat(newPath, nextPath);
		}
		
		nextPath = strtok(NULL, ":");
	}
	free(MYPATH);
	MYPATH = newPath;
	setenv("PATH", MYPATH, 1);
}

void MyPath(char *args[], int arg_count){
	char *usageError = "Usage error: path [+|-] PATH\n"; 
	if (arg_count == 2 || arg_count > 3)
	{
		printf("%s", usageError);
		return;
	}
	if (arg_count == 1)
	{
		printf("%s\n", MYPATH);
		return;
	}
	if (arg_count == 3)
	{
		int isAdd = strcmp(args[0], "+");
		int isRemove = strcmp(args[0], "-");
		if (isAdd != 0 && isRemove != 0)
		{
			printf("%s", usageError);
			return;
		}
		if (isAdd == 0)
		{
			AddPath(args[1]);
			return;
		}
		if (isRemove == 0)
		{
			RemovePath(args[1]);
			return;
		}
	}
}

void MyHistory(char *args[], int arg_count){
	int ind;
	if (hist_count < 20)
		ind = hist_count;
	else 
		ind = 20;


	if (arg_count == 1){  
		//printf("History count is: %d\n",hist_count);
		for (int i=0; i<ind; i++){
			printf("%d.%s\n", i+1,MYHISTORY[i]);
		}
	}else if(arg_count == 2 && strcmp(args[0],"-c") == 0){
		for(int i=0; i<ind; i++){
			if(MYHISTORY[i] != NULL){
				free(MYHISTORY[i]);
				MYHISTORY[i] = NULL;
			}
		}
		hist_count = 0;//reset history count

	}else if(arg_count == 3 && strcmp(args[0],"-e") == 0){

		int index = atoi(args[1]);
		if (index < hist_count){
			int cmd_num = ParseCommands(MYHISTORY[index-1]);
			
			//execute commands that are saved in COMMANDS[] array
			for(int i=0; i < cmd_num; i++){
				char *temp = strdup(COMMANDS[i]);
				temp = strtok(temp, " ");
				ExecuteCommands(temp, COMMANDS[i]);
				//free temp
				free(temp);
			}
		}else{
			printf("Number entered is not in range for commands in history.\n");
		}
	} 
}

void PipeCommands(char *args[], char *first_command, int arg_count){
	//declare pipes and pids
    int fd1[2];
	int fd2[2];
	pid_t pid;
	int status;
	int wpid;
	//declare command arrays and variables to use  
	char *command1[20] = {NULL};
	char *command2[20] = {NULL};
	char *command3[20] = {NULL};
	int count = 0;
	int backup;
	//get the first command from the args array
	command1[0] = first_command;
	count++;
	for (int i = 0; i < arg_count-1; ++i){
		if (strcmp(args[i],"|") == 0){
			backup = i+1;
			break;
		}
		command1[count++] = args[i];
	}
	count = 0;

	//get the second command from the args array
	for (int i = backup; i < arg_count-1; ++i){
		if (strcmp(args[i],"|") == 0){
			backup = i+1;
			break;
		}
		command2[count++] = args[i];
	}

	count = 0;
	//get the third command from the args array
	for (int i = backup; i < arg_count-1; ++i){
		if (strcmp(args[i],"|") == 0){
			backup = i+1;
			break;
		}
		command3[count++] = args[i];
	}



	if ( pipe(fd1) == -1 ){//call pipe command
	
		perror("Bad pipe.\n");
		exit (1);
	}

	if( (pid = fork()) < -1 ){ // if the value of the process id (pid) after fork is les than -1 then an error occured
	
		perror("Error forking a child.\n");
		exit (1);
	}
	else if ( pid == 0 ){  // if pid equals 0 in child
	
		close(fd1[0]); // close reading pipe
		dup2(fd1[1], fileno(stdout)); //use dup2 to dup wite to pipe instead of stdout
		close(fd1[1]); // close writing pipe
		execvp(command1[0], command1);  // Execute command1
		_exit(0);
	}else{ // in the parent 
		do{
			wpid = waitpid(pid, &status, WUNTRACED);
		}while(!WIFEXITED(status) && !WIFSIGNALED(status)); // wait for the child to terminate
		if( WIFEXITED(status) ){ // once the first child process terminates go in this if block
		
			if(pipe(fd2) == -1){ //open the second pipe
			
				perror("Bad pipe.\n");
				exit (1);
			}
			pid = fork(); // fork another child 
			if(pid < 0){  // check for error
			
				perror("Error forking a child.\n");
				exit (1);
			}
			else if ( pid == 0){  // in the child 
			
				//dup2(std_out_dup,1); //-------------------------------
				if(command3[0] != NULL)  // if there is a third command piped with the other two
				{
					close(fd1[1]);  // close writing pipe for the first pipe
					close(fd2[0]);  // close reading pipe for the second pipe
					dup2(fd1[0], fileno(stdin));  // instead of using input from stdin just use input from first command output
					dup2(fd2[1], fileno(stdout));  //instead of print out in file descriptor stdout, put the output in pipe
					close(fd1[0]);  // close reading pipe for first pipe
					close(fd2[1]);  // close writing pipe for second pipe
					execvp(command2[0], command2);  // execute command 2
				}
				else  // if there is no third command just execute second command
				{
					close(fd1[1]);
					dup2(fd1[0], fileno(stdin));
					close(fd1[0]);
					execvp(command2[0], command2);
				}
			
				_exit (0);
			}else{
			
			
				close(fd1[0]);  // close reading pipe for the first pipe
				close(fd1[1]);  // close writing pipe for the first pipe

				do
				{
					wpid = waitpid(pid, &status, WUNTRACED);
				} while(!WIFEXITED(status) && !WIFSIGNALED(status)); // wait for process that executes the second command to terminate
				if(command3[0] != NULL){  // if there is a third command
				
					pid = fork();  // fork a third process
					if(pid < 0){  // checking for forking errors
					
						perror("Error forking.\n");
						exit(1);
					}
					else if (pid == 0){  // inside child process
					
						close(fd2[1]);  // close writing pipe
						dup2(fd2[0], fileno(stdin));  // use pipe as input for third command
						close(fd2[0]);  // close reading pipe
						execvp(command3[0], command3);  // execute command 3
						_exit(1);//-------------------------------
					}
					else{  // in the parent process
					
						close(fd2[0]);  // close reading pipe for second pipe
						close(fd2[1]);  // close writing pipe for second pipe
						do{
							wpid = waitpid(pid, &status, WUNTRACED);
						} while(!WIFEXITED(status) && !WIFSIGNALED(status));  // wait for child process to terminate
					}
				}
			}
		}
	}
}

void signalHandle(int sig){
	// Only handle signals when a child process is running
	if (cpid == 0) return;

	pid_t pid = fork();
	if (pid != 0) {
		waitpid(pid, NULL, 0);
		return;
	}

	char *sSig;
	if (sig == SIGINT) 
		sSig = "INT";
	else if (sig == SIGTSTP)
		sSig = "STOP";
	else return;
	
	char *sgPid = malloc(sizeof(char) * 8);
	
	// Negate to send signal to all processes in group
	sprintf(sgPid, "%d", cpid);
	
	char *args[] = { "kill", "-s", sSig, sgPid};
	execvp("kill", args);
}
			
void io_redirect(char *command, char* full_line)
{
	char *args[MAX]; 
	char *filename; 
	char *backup_line = strdup(full_line);
	int arg_count = ParseArgs(full_line, args);
	char *command_args[arg_count-2];
	strcpy(full_line, backup_line);
    free(backup_line);
	
	int output_redirect, input_redirect = 0;

	if (strcmp(COMMANDS[0],"<") == 0 || strcmp(COMMANDS[0],">") == 0) 
	{
		fprintf(stderr,"Needs a command\n");		
		return;	
	}
	else if (args[arg_count-2][0] == '<' || args[arg_count-2][0] == '>') 
	{
		fprintf(stderr,"Include file\n");
		return;	
	}
	command_args[0] = COMMANDS[0];
	if (strchr(full_line, '<') != NULL) 
	{
		output_redirect = 1;
		int i = 0;
		while (strcmp(args[i],"<")) 
		{
			command_args[i+1] = args[i];
			i++; 
		}	
	}
	else if (strchr(full_line, '>') != NULL) 
	{
		input_redirect = 1;
		int i = 0;
		while (strcmp(args[i],">")) 
		{
			command_args[i+1] = args[i];
			i++; 
		}	
	}
	else 
	{
		fprintf(stderr, "ERROR\n");
		return;
	}
	command_args[arg_count-2] = '\0';
	pid_t id = fork();
	if(id == 0) 
	{
      	if(output_redirect)
		{
			filename = args[arg_count-2];
            int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0)
			{
				printf("Couldn't run\n");	
				exit(0);
			}
            dup2(fd, 1);
			close(fd);
            execvp(command_args[0], command_args);
			fprintf(stderr,"%s: not found\n",args[0]);
			exit(0);
		}
		else if (input_redirect)
		{
			filename = args[arg_count-2];
            int fd = open(filename, O_RDONLY);
            if (fd < 0)
			{
				printf("Couldn't run\n");
				exit(0);
			}
            dup2(fd, 0);
			close(fd);
            execvp(command_args[0], command_args);
			fprintf(stderr,"%s: not found\n",args[0]);
			exit(0);	
		}	
		
		else 
		{	fprintf(stderr,"Redirection failed\n");	
			exit(0);	
		}
	}
	else if (id > 0) 
	{
		wait(NULL);	
	}
	else 
	{
		fprintf(stderr,"Fork Error\n");	
	}			            
}

void MyAlias(char *args[], int arg_count){

}
