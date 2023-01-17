# TinyFS Project

## Description

A small file system written in C for a virtual disk, tinyfs. Project from operating systems course at Cleveland State University. File modified for course project is 'fs.c'. File system is capable of searching valid data blocks through a virtual image's superblock, deleting files in virtual images by unlinking block pointers and freeing inode, and printing the contents of a file contained in the virtual image to the console or to a file.

The file system is similar to the Unix file system. A TinyFS image contains a superblock, four inode tables, and several data blocks. These data blocks can be utilized as a root directory (contains information for files as entries), as an indirect pointer block (more pointers in case a file has used up the five direct pointers), or as a raw data block.