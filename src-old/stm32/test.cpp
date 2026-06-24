
extern "C" int printf(char const*, ...);

namespace Test {
void hello(){
	printf(">> Hello from C++\r\n");
}
}

extern "C" {
void cxx_test(){
	Test::hello();
}
}
