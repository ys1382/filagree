javac JniSample.java 
javah -jni JniSample
cc -c -fPIC -I/System/Library/Frameworks/JavaVM.framework/Headers JniSample.c -o libJniSample.o
libtool -dynamic -lSystem libJniSample.o -o libJniSample.dylib -macosx_version_min 10.8
LD_LIBRARY_PATH=.
