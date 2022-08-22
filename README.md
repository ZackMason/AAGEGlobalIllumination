# AAGEGlobalIllumination

CPU GI, working on moving to compute shader

![image](https://user-images.githubusercontent.com/3623261/185831810-c542058d-6364-40e6-b2db-a703e47c9dcf.png)

This template was created using `cpp_init.py` https://github.com/ZackMason/cpp_init

# How To Build 

```
mkdir build
cd build
conan install .. -s build_type=Release
cmake ..
cmake --build . --config Release
```

follow conan instructions if this is your first time building AAGE, you probably need to add --build=missing
