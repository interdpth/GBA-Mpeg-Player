int rawread(unsigned char* * src, unsigned char* dst, int size)
{
	unsigned char* srcp = (*src);
	for (int i = 0; i < size; i++)
	{
		dst[i] = srcp[i];
	}

	(*src) += size;

	return size;
}
int overallSize;
void* customalloc(int size)
{
	overallSize += size;
	return malloc(size);
}




void lseek();

//int fclose(int a)
//{
//	return 0;
//}

void exit(int b)
{
	while (1);
}

int scustomprint(char* str, const char* format, ...)
{
	char* newptr = str;
	int c = 0;
	while (*newptr != 0) {
		(*((unsigned char*)(0x5000000 + c)) = *newptr++);	
}
	return 0;
}

int customprint(char* str, const char* format, ...)
{
	scustomprint(str, format);
	return 0;
}

int fcustomprint(char* str1, char* str, const char* format, ...)
{
	scustomprint(str, format);
	return 0;
}