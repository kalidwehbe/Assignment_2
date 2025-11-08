if [ ! -d "bin" ]; then
    mkdir bin
else
	rm bin/*
fi
g++ -g -O0 -I . -o bin/interrupts interrupts_101259994_101108918.cpp