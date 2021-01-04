# Shello v0.2
### A simple unix-like shell implementation in C/C++

### Provides:
* History support via GNU readline
* Internal (built-in) commands : 
    * cd;
    * wc, with options -c -w -l -L;
    * tee, with option -a.
* Multiple pipe support    
* Simple redirection (piped redirection might not function properly)