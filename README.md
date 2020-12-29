# smsh
A simple shell.  
Description:
- Quoting
    - All characters enclosed in single and double quotes preserve their literal meaning.
- Parameter expansion
    - The only supported form of the parameter expansion is $parameter.
 - Arithmetic expansion
    - The only supported form of the arithmetic expansion is $((..)).
 - Redirection
    - '>' redirects standart output.
    - '<' redirects standart input.
 - Pipelines
    - command1 [ | command2 ...]
 - Lists
    - Asynchronous lists. If a command is terminated by the '&' it will be run in the background.  
    command1 & [command2 & ... ]
    - Sequential lists. Commands that are separated by a ';' will be executed sequentially.  
    command1 [; command2] ...
 - Compound commands
    - for loop
    - while loop
    - if/else statement
  - Builtin commands
    - cd
    - export
    - unset
    - help
    - bg
    - fg
- [Grammar](https://github.com/3axapMaiceenka/smsh/blob/main/doc/grammar.txt)
