@echo off
git rev-parse HEAD > git_sha.temp
set /p GIT_SHA_LONG=<git_sha.temp

git rev-parse --short HEAD > git_sha_short.temp
set /p GIT_SHA_SHORT=<git_sha_short.temp

(
@echo #ifndef __GIT_SHA_H_
@echo #define __GIT_SHA_H_
@echo const char * build_date = "%DATE%";
@echo const char * build_time = "%TIME%";
@echo const char * build_git_sha = "%GIT_SHA_LONG%";
@echo const char * build_git_sha_short = "%GIT_SHA_SHORT%";
@echo #endif __GIT_SHA_H_
) > ..\Inc\git_sha.h
