#include <ctype.h>

static char limpiarPantalla[] = "\e[1;1H\e[2J";

//funcion que determina si hilera es solo numero entero
int esNumerico(char s[])
{
	int i;
	int largo = strlen(s);

	for (i = 0; i < largo; i++)
	{
		if (!isdigit(s[i]))
		{
			return 0;
		}
	}
	return 1;
}

//funcion que convierte numero en ascii
char* miItoa(int num, char *s)
{
	if (s == NULL)
	{
		return NULL;
	}
	sprintf(s, "%d", num);
	return s;
}


int esPotencia(int base, int numero)
{
	if (numero == 1)
	{
		return 0;
	}
	int actual = base;
	
	while (actual < numero){
		actual = actual * base;
	}
	
	if (actual == numero){
		return 1;
	}else{
		return 0;
	}
}

//funcion que retorna un timestamp como string
char* timestamp()
{
	time_t tiempo;
	tiempo = time(NULL);
	return asctime(localtime(&tiempo));
}
