# A window that can bounce around written with win32 in c++

https://github.com/user-attachments/assets/b2ae3688-0e11-4884-86de-e36693328b1a

## how 2 build
open bounce2.sln in visual studio 2022

## how 2 contribute
pr.

## q&a

Q: Why does this exist?  
A: I saw a cool demo on tiktok.

Q: Can i throw it around?  
A: Yes. That's why it has so many commits.

Q: How does it work?  
A: Simple, first get the screen dimentions, then the "usable" screen dimentions, subtract one from another and you get the taskbar height.  
Then, get the virtual screen resolution and check collision against the rect. A simple timer does the job. Leftside collision needs an ofset of 7. God help us.

![tenor (4)](https://github.com/user-attachments/assets/27ffa153-483c-4ef6-a8b2-881d845f1484)
