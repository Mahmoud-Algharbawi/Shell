int main()
{
char s[100];

int c;

// printing current working directory
printf("%s\n", getcwd(s, 100));

printf("Enter 1 to go required directory \n Enter 2 to go home directory\n");

scanf("%d",&c);

switch(c)

{


if(chdir(s)!=0)

perror("chdir() to failed");

break;

case 2: // change the directory to home directory

chdir("..");

break;

default : printf("Enter valid input\n");

// using the command
}

// printing current working directory
printf("%s\n", getcwd(s, 100));

// after chdir is executed
return 0;
}


CODE ALTERNATIVE WITHOUT INT


scanf(" %s",s);

// using command and checking is that directory is the or not

if(chdir(s)!=0)

perror("chdir() to failed");

break;

case 2: // change the directoty to home directory

chdir("..");

break;

default : printf("Enetr valid input\n");

// using the command
}

// printing current working directory
printf("%s\n", getcwd(s, 100));

// after chdir is executed
return 0;



