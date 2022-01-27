int rawread(unsigned char** src, unsigned char* dst, int size)
{

	for (int i = 0; i < size; i++)
	{
		dst[i] = *src[i];
	}

	*src += size;

	return size;
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

int sprintf(char* str, const char* format, ...)
{
	while (1);
	return 0;
}

int printf(const char* format, ...)
{
	return 0;
}

int fprintf(void* str, const char* format, ...)
{
	while (1);
	return 0;
}