#pragma once
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <dirent.h>
#include <stdio.h>

using std::vector;
using std::string;


// Lists all the files in the specified directory. If not given an argument,
// the current working directory is used instead.
int com_ls(vector<string>& tokens);

int com_clear(std::vector<string>& tokens);

// Changes the current working directory to that specified by the given
// argument.
int com_cd(vector<string>& tokens);


// Displays the current working directory.
int com_pwd(vector<string>& tokens);


// If called without an argument, then any existing aliases are displayed.
// Otherwise, the second argument is assumed to be a new alias and an entry
// is made in the alias map.
int com_alias(vector<string>& tokens);


// Removes aliases. If "-a" is provided as the second argument, then all
// existing aliases are removed. Otherwise, the second argument is assumed to
// be a specific alias to remove and if it exists, that alias is deleted.
int com_unalias(vector<string>& tokens);


// Prints all arguments to the terminal.
int com_echo(vector<string>& tokens);


// Exits the program.
int com_exit(vector<string>& tokens);


// Displays all previously entered commands, as well as their associated line
// numbers in history.
int com_history(vector<string>& tokens);


// Returns the current working directory.
string pwd();
