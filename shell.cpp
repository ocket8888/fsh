#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
//#include <locale.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "builtins.h"

// Potentially useful #includes (either here or in builtins.h):
//   #include <dirent.h>
//   #include <errno.h>
//   #include <fcntl.h>
//   #include <signal.h>
#include <sys/errno.h>
//   #include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

map<string,string> aliases;

// The characters that readline will use to delimit words
const char* const WORD_DELIMITERS = " \t\n\"\\'`@><=;|&{(";

// An external reference to the execution environment
extern char** environ;

// Define 'command' as a type for built-in commands
typedef int (*command)(vector<string>&);

// A mapping of internal commands to their corresponding functions
map<string, command> builtins;

// Variables local to the shell
map<string, string> localvars;

HIST_ENTRY **the_history_list;



// Handles external commands, redirects, and pipes.
int execute_external_command(vector<string>& tokens) {
  string command=tokens[0];
  bool bkgrnd=false;
  char* args[tokens.size()+1];
  char* envpath=getenv("PATH");
  char* path=(char*)(malloc(sizeof(char)*1024));
  strcpy(path,envpath);
  char delim=':';
  while(path!=NULL){
    char* filetotest=strsep(&path,&delim);
    string file=filetotest;
    file.append("/");
    file.append(command);
    if(access(file.c_str(),F_OK)==0){
      args[0]=(char*)(malloc(file.length()*sizeof(char)));
      strcpy(args[0],file.c_str());
      for(unsigned int i=1;i<tokens.size();i++){
        if(tokens[i]=="&"){
          bkgrnd=true;
          continue;
        }
        args[i]=(char*)(malloc(tokens[i].length()*sizeof(char)));
        strcpy(args[i],tokens[i].c_str());
      }
      if (bkgrnd)
      {
        args[tokens.size()-1]=NULL;
        FILE* fp=fopen("/dev/null","w");
        dup2(fileno(fp),1);
        close(fileno(fp));
      }
      else{
        args[tokens.size()]=NULL;
      }
      int cpid=fork();
      switch(cpid){
        case -1:
          //Error
          perror("ERROR: could not fork\n");
          return -1;
        case 0:
          //child
          execvp(args[0],args);
          perror("exec failure");
          return -1;
        default:
          //parent
          int status=0;
          if (!bkgrnd)
          {
            wait(&status);
          }
          return status;
      }
    }
  }
  fprintf(stderr, "%s: Command not found\n",command.c_str());
  return 1;
}


// Return a string representing the prompt to display to the user. It needs to
// include the current working directory and should also use the return value to
// indicate the result (success or failure) of the last command.
string get_prompt(int return_value) {
  string prompt="";
  if(return_value!=0){
    prompt="X ";
  }
  prompt+=pwd()+"> ";
  return prompt;
}


// Return one of the matches, or NULL if there are no more.
char* pop_match(vector<string>& matches) {
  if (matches.size() > 0) {
    const char* match = matches.back().c_str();

    // Delete the last element
    matches.pop_back();

    // We need to return a copy, because readline deallocates when done
    char* copy = (char*) malloc(strlen(match) + 1);
    strcpy(copy, match);

    return copy;
  }

  // No more matches
  return NULL;
}





bool startsWith(string word,string init){
  return word.substr(0, init.size())==init;
}

bool exists(string str,string delim){
  string::size_type loc=str.find(delim, 0);
  return loc!=string::npos;
}
//Split a str by char into a vector
vector<string> split(string str, char delim){
  stringstream ss(str);
  string item;
  vector<string> retObj;
  while(getline(ss, item, delim)){
    if(item != "") retObj.push_back(item);
  }
  return retObj;
}




// Generates environment variables for readline completion. This function will
// be called multiple times by readline and will return a single cstring each
// time.
char* environment_completion_generator(const char* text, int state) {
  // A list of all the matches;
  // Must be static because this function is called repeatedly
  static vector<string> matches;

  // If this is the first time called, construct the matches list with
  // all possible matches
  if (state == 0) {
    for(unsigned int i=0;i<sizeof(environ);i++){
      vector<string> toks=split(string(environ[i]),'=');
      string var = toks[0];

      if(startsWith("$" + var, string(text))){
        matches.push_back("$" + var);
      }
    }
    for (map<string, string>::iterator it = localvars.begin(); it != localvars.end(); it++) {
      if (startsWith("$" + (it->first), string(text))) {
        matches.push_back("$" + string(it->first));
      }
    }
  }

  // Return a single match (one for each time the function is called)
  return pop_match(matches);
}

// Generates commands for readline completion. This function will be called
// multiple times by readline and will return a single cstring each time.
char* command_completion_generator(const char* text, int state) {
  // A list of all the matches;
  // Must be static because this function is called repeatedly
  static vector<string> matches;

  // If this is the first time called, construct the matches list with
  // all possible matches
  if (state == 0) {
   for(map<string, command>::iterator it = builtins.begin(); it != builtins.end(); ++it){
      if(startsWith(it->first, string(text))) matches.push_back(it->first);
    }

    string path = string(getenv("PATH"));
    vector<string> paths = split(path, ':');
    for(unsigned int i = 0; i < paths.size(); ++i){
      //cout << paths[i] << endl;
      DIR* curDir = opendir(paths[i].c_str());
      if(curDir){
        struct dirent *pDirent;
        while ((pDirent = readdir(curDir)) != NULL) {
          if(startsWith(pDirent->d_name, string(text))) matches.push_back(string(pDirent->d_name));
        }
      }
      else{
        perror("Cannot open dir");
      }
      closedir(curDir);
    }
  }

  // Return a single match (one for each time the function is called)
  return pop_match(matches);
}


// This is the function we registered as rl_attempted_completion_function. It
// attempts to complete with a command, variable name, or filename.
char** word_completion(const char* text, int start, int end) {
  char** matches = NULL;

  if (start == 0) {
    rl_completion_append_character = ' ';
    matches = rl_completion_matches(text, command_completion_generator);
  } else if (text[0] == '$') {
    rl_completion_append_character = ' ';
    matches = rl_completion_matches(text, environment_completion_generator);
  } else {
    rl_completion_append_character = '\0';
    // We get directory matches for free (thanks, readline!)
  }

  return matches;
}


// Transform a C-style string into a C++ vector of string tokens, delimited by
// whitespace.
vector<string> tokenize(const char* line) {
  vector<string> tokens;
  string token;

  // istringstream allows us to treat the string like a stream
  istringstream token_stream(line);

  while (token_stream >> token) {
    tokens.push_back(token);
  }

  // Search for quotation marks, which are explicitly disallowed
  for (size_t i = 0; i < tokens.size(); i++) {

    if (tokens[i].find_first_of("\"'`") != string::npos) {
      cerr << "\", ', and ` characters are not allowed." << endl;

      tokens.clear();
    }
  }

  return tokens;
}

char* detokenize(const vector<string> tokens){
  string temp=""; //shut up
  for(unsigned int i=0;i<tokens.size();i++){
    temp+=tokens[i];
    if(i!=tokens.size()-1){
      temp+=" ";
    }
  }
  char* final=(char*)(malloc(temp.length()*sizeof(char)));
  strcpy(final,temp.c_str());
  return final;
}

char* detokenize(const vector<string> tokens,const int index, const string replace){
  char temp[1024];
  for(int i=0;i<index;i++){
    strcat(temp,tokens[i].c_str());
    strcat(temp," ");
  }
  strcat(temp,replace.c_str());
  strcat(temp," ");
  for(unsigned int j=index+1;j<tokens.size();j++){
    strcat(temp,tokens[j].c_str());
    if(j!=tokens.size()-1){
      strcat(temp," ");
    }
  }
  char* final=temp;
  return final;
}
int execute_line(vector<string>&,map<string,command>&);
int pipeit(vector<string> tokens,unsigned int index){
	vector<string> firstline;
	vector<string> secondline;
	for(unsigned int i=0;i<tokens.size();i++){
		if(i==index){
			continue;
		}
		else if(i<index){
			firstline.push_back(tokens[i]);
		}
		else{
			secondline.push_back(tokens[i]);
		}
	}
	

  //redirect from a file
	if(tokens[index]=="<"){
		if(secondline.size()!=1){
			fprintf(stderr,"ERROR: expected exactly one file descriptor as pipe input\n");
			return -1;
		}
		else if(access(secondline[0].c_str(),R_OK | F_OK)!=0){
			perror("myshell:");
      return -1;
		}
    char* fname=(char*)(malloc(sizeof(char)*secondline[0].length()-1));
    strcpy(fname,secondline[0].c_str());
    const char mode='r';
    const char* modeptr=&mode;
    FILE* fp=fopen(fname,modeptr);
    int stdIn=dup(0);
		dup2(fileno(fp),0);
    close(fileno(fp));
    int return_value=execute_line(firstline,builtins);
    dup2(stdIn,0);
    close(stdIn);
    return return_value;
	}

  //redirect to a file
  else if(tokens[index]==">"){
    if(secondline.size()!=1){
      fprintf(stderr,"ERROR: expected exactly one file descriptor as pipe output\n");
      return -1;
    }
    char* fname=(char*)(malloc(sizeof(char)*secondline[0].length()-1));
    strcpy(fname,secondline[0].c_str());
    const char mode='w';
    const char* modeptr=&mode;
    FILE* fp=fopen(fname,modeptr);
    int stdOut=dup(1);
    dup2(fileno(fp),1);
    close(fileno(fp));
    int return_value=execute_line(firstline,builtins);
    dup2(stdOut,1);
    close(stdOut);
    return return_value;
  }

  //append to a file
  else if(tokens[index]==">>"){
    if(secondline.size()!=1){
      fprintf(stderr,"ERROR: expected exactly one file descriptor as pipe output\n");
      return -1;
    }
    char* fname=(char*)(malloc(sizeof(char)*secondline[0].length()-1));
    strcpy(fname,secondline[0].c_str());
    const char mode='a';
    const char* modeptr=&mode;
    FILE* fp=fopen(fname,modeptr);
    int stdOut=dup(1);
    dup2(fileno(fp),1);
    close(fileno(fp));
    int return_value=execute_line(firstline,builtins);
    dup2(stdOut,1);
    close(stdOut);
    return return_value;
  }

  //command piping
  else if(tokens[index]=="|"){
    if(secondline.size()==0||firstline.size()==0){
      fprintf(stderr, "ERROR, expected at least one command at both ends of pipe\n");
    }
    int pip[2];
    if(pipe(pip)<0){
      perror("pipe failure");
      return -1;
    }
    int parentstatus=0;
    int childstatus=0;
    switch(fork()){
      case -1:
        {
          close(pip[0]);
          close(pip[1]);
          perror("fork failure");
          return -1;
        }
      case 0:
        {
          //child
          close(pip[1]);
          int stdIn=dup(0);
          dup2(pip[0],0);
          close(pip[0]);
          execute_line(secondline,builtins);
          dup2(stdIn,0);
          close(stdIn);
          break;
        }
      default:
        {
          //parent
          close(pip[0]);
          int stdOut=dup(1);
          dup2(pip[1],1);
          close(pip[1]);
          parentstatus=execute_line(firstline,builtins);
          dup2(stdOut,1);
          close(stdOut);
          int status;
          wait(&status);
          childstatus=status;
          break;
        }
    }
    if(childstatus!=0){
      return childstatus;
    }
    else if(parentstatus!=0){
      return parentstatus;
    }
    return 0;
  }

	return 0;
}


// Executes a line of input by either calling execute_external_command or
// directly invoking the built-in command.
int execute_line(vector<string>& tokens, map<string, command>& builtins) {
  int return_value = 0;

  if (tokens.size() != 0) {
    //replaces aliases with their values before executing
    for(unsigned int i=0;i<tokens.size();i++){
      map<string, string>::iterator alias=aliases.find(tokens[i]);
      if(alias!=aliases.end()){
        tokens[i]=aliases[tokens[i]];
      }
    }

    
    while(true){
      bool replaced=false;
      //replaces bangs with commands
      for(unsigned int i=0;i<tokens.size();i++){
        if(tokens[i][0]=='!'&&tokens[i][1]=='!'){
          HIST_ENTRY* lastCMD=previous_history();
	  char* newTokens=detokenize(tokens,i,lastCMD->line);
	  tokens=tokenize(newTokens);
	  history_set_pos(history_length-1);
	  replaced=true;
	  break;
	}
	else if(tokens[i][0]=='!'){
		string number=tokens[i].substr(1,tokens[i].length()-1);
		int offset=history_length-atoi(number.c_str());
		history_set_pos(offset);
		HIST_ENTRY* lastcmd=current_history();
		char* newtokens=detokenize(tokens,i,lastcmd->line);
		tokens=tokenize(newtokens);
		history_set_pos(history_length-1);
		replaced=true;
		break;
	}
      }
      if(!replaced){
	      break;
      }
    }

    bool pipe=false;
    unsigned int index;
    for(unsigned int i=0;i<tokens.size();i++){
	    if(tokens[i]=="|"||tokens[i]=="<"||tokens[i]==">"||tokens[i]==">>"){
		    pipe=true;
		    index=i;
		    break;
	    }
	    if(pipe)break;
    }
    if(pipe){
	    return pipeit(tokens,index);
    }

    map<string, command>::iterator cmd = builtins.find(tokens[0]);\
    if (cmd == builtins.end()) {
	    return_value = execute_external_command(tokens);
    }
    else{
	    return_value = ((*cmd->second)(tokens));
    }
  }
  return return_value;
}


// Substitutes any tokens that start with a $ with their appropriate value, or
// with an empty string if no match is found.
void variable_substitution(vector<string>& tokens) {
	vector<string>::iterator token;

	for (token = tokens.begin(); token != tokens.end(); ++token) {

		if (token->at(0) == '$') {
			string var_name = token->substr(1);

			if (getenv(var_name.c_str()) != NULL) {
				*token = getenv(var_name.c_str());
			} else if (localvars.find(var_name) != localvars.end()) {
				*token = localvars.find(var_name)->second;
			} else {
				*token = "";
			}
		}
	}
}


// Examines each token and sets an env variable for any that are in the form
// of key=value.
void local_variable_assignment(vector<string>& tokens) {
	vector<string>::iterator token = tokens.begin();

	while (token != tokens.end()) {
		string::size_type eq_pos = token->find("=");

		// If there is an equal sign in the token, assume the token is var=value
		if (eq_pos != string::npos) {
			string name = token->substr(0, eq_pos);
			string value = token->substr(eq_pos + 1);

			localvars[name] = value;

			token = tokens.erase(token);
		} else {
			++token;
		}
	}
}


// The main program
int main() {
	// Populate the map of available built-in functions
	builtins["ls"] = &com_ls;
	builtins["cd"] = &com_cd;
	builtins["pwd"] = &com_pwd;
	builtins["alias"] = &com_alias;
	builtins["unalias"] = &com_unalias;
	builtins["echo"] = &com_echo;
	builtins["exit"] = &com_exit;
	builtins["history"] = &com_history;

	// Specify the characters that readline uses to delimit words
	rl_basic_word_break_characters = (char *) WORD_DELIMITERS;

	// Tell the completer that we want to try completion first
	rl_attempted_completion_function = word_completion;

	// The return value of the last command executed
	int return_value = 0;

	// Loop for multiple successive commands 
	while (true) {

		// Get the prompt to show, based on the return value of the last command
		string prompt = get_prompt(return_value);

		// Read a line of input from the user
		char* line = readline(prompt.c_str());

		// If the pointer is null, then an EOF has been received (ctrl-d)
		if (!line) {
			break;
		}

		// If the command is non-empty, attempt to execute it
		if (line[0]) {

			// Add this command to readline's history
			add_history(line);

			// Break the raw input line into tokens
			vector<string> tokens = tokenize(line);

      // Handle local variable declarations
      if(tokens[0]!="alias"){
        local_variable_assignment(tokens);
      }

      // Substitute variable references
      variable_substitution(tokens);

      // Execute the line
      return_value = execute_line(tokens, builtins);
    }

    // Free the memory for the input string
    free(line);
  }

  return 0;
}
