#define MAXBUFFER 50
#define MASCARA 100

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include "funcionesAuxiliares.c"

struct parametrosProcesoPagina {
	int id;
	int numPaginas;
	int tiempo;
};

struct parametrosProcesoSegmento {
	int id;
	int numSegmentos;
	int espacioPorSegmento;
	int tiempo;
};

key_t clave;
sem_t semaforo;
int idMemoria;
int *memoria;

int colaEspera[MAXBUFFER] = { 0 };

char colaEjecutados[] = "";
char colaMuertos[] = "";

int procesoBuscando;

unsigned int continuar = 1;

int longitudPagina;
int cantPaginas;
int maxMemoriaCompartida;
int modoOperacionPagina; 	//1 = paginacion, 0=segmentacion


pthread_t hiloCreadorProcesos;

FILE *bitacora;

unsigned int pedirDatosMemoria()
{
	printf("%s", limpiarPantalla);
	char buffer[MAXBUFFER];
	
	printf("Ingrese el numero de cantidad de espacios de memoria: ");
	fgets(buffer,MAXBUFFER, stdin);
	buffer[strlen(buffer) - 1] = '\0';
	if (!esNumerico(buffer)){
		printf("Debe ingresar un valor numerico y entero\n");
		continuar = 0;
		return continuar;
	}

	maxMemoriaCompartida = atoi(buffer);
	if (maxMemoriaCompartida == 0)
	{
		printf("Debe ingresar un valor mayor a 0\n");
		continuar = 0;
	
	}else if (!esPotencia(2, maxMemoriaCompartida)){
		printf("Debe ingresar una cantidad que sea potencia de 2\n");
		continuar = 0;
	}
	return continuar;
}

unsigned int pedirCantPaginas()
{
	char buffer[MAXBUFFER];
	printf("Ingrese el numero de espacios que toma una pagina: ");
	fgets(buffer,MAXBUFFER, stdin);
	buffer[strlen(buffer) - 1] = '\0';
	if (!esNumerico(buffer)){
		printf("Debe ingresar un valor numerico y entero\n");
		continuar = 0;
		return continuar;
	}
	
	longitudPagina = atoi(buffer);
	if (longitudPagina == 0)
	{
		printf("Debe ingresar un valor mayor a 0\n");
		continuar = 0;
		
	}else if (longitudPagina > maxMemoriaCompartida)
	{
		printf("La longitud de una pagina no cabe en la memoria\n");
		continuar = 0;
	}else{
		cantPaginas = (int)(maxMemoriaCompartida / longitudPagina);
		printf("La memoria trabajara con %d paginas\n", cantPaginas);
	}
	return continuar;
}


unsigned int reservarMemoria()
{
	//conseguimos una clave para la memoria compartida
	//Todos los procesos que quieran compartir la memoria
	//  deben obtener la misma clave.
	clave = 9876;
	
	//Creamos la memoria con la clave. Se usa shmget pasando:
	//   clave, tamano de memoria, y flags
	//Los flags son permisos de lectura/escritura/ejecucion
	//Para propietario, grupo y otros es 0777
	//Flag IPC_CREAT indice que se cree la memoria
	//La funcion retorna un id para la memoria recien creada
	idMemoria = shmget(clave, sizeof(int)*maxMemoriaCompartida, 0777 | IPC_CREAT);
	if (idMemoria == -1)
	{
		printf("No logre conseguir id para memoria compartida\n");
		continuar = 0;
		return continuar;
	}
	
	//Una vez creada la memoria, se hace que de los punteros apunte a
	//la zona de memoria creada. Para esto llamamos shmat, pasandole id,
	
	memoria = (int *)shmat(idMemoria, (char *)0, 0);
	if (memoria == NULL)
	{
		printf("No consegui memoria compartida\n");
		continuar = 0;
		return continuar;
	}
	return continuar;
	
	//Ya podemos usar la memoria compartida
}

unsigned int crearSemaforo()
{
	//el segundo parametro es para indicar si va a ser compartido
	//   entre threads(0) o procesos(1)
	
	//el tercer parametro es para inicializar el semaforo con un valor
	if (sem_init(&semaforo, 0, 1) == -1){
		return 0;
	}else{
		return 1;
	}
}


void liberarMemoria()
{
	shmdt(memoria);
	shmctl(idMemoria, IPC_RMID, (struct shmid_ds *)NULL);
	printf("La memoria se ha liberado con exito\n");
}

void liberarSemaforo()
{
	sem_destroy(&semaforo);
	printf("Semaforo liberado con exito\n");
}

unsigned int inicializador()
{
	if (pedirDatosMemoria()){
		if (pedirCantPaginas()){
			if (reservarMemoria()){
				printf("He reservado la memoria\n");
				if (crearSemaforo()){
					continuar = 1;
				}else{
					continuar = 0;
				}
			}
		}
	}
	return continuar;
}

unsigned int pedirModoOperacion()
{
	printf("===============\n");
	printf("1. Modo paginacion\n");
	printf("2. Modo segmentacion\n");
	char opcion[MAXBUFFER];
	fgets(opcion,MAXBUFFER, stdin);
	opcion[strlen(opcion) - 1] = '\0';
	if (!esNumerico(opcion)){
		printf("Debe ingresar un valor numerico y entero\n");
		continuar = 0;
		return continuar;
	}else{
		int opcionSeleccionada = atoi(opcion);
		if (opcionSeleccionada == 1)
		{
			modoOperacionPagina = 1;
			continuar = 1;
			
		}else if(opcionSeleccionada == 2)
		{
			modoOperacionPagina = 0;
			continuar = 1; 
		}else{
			printf("Opcion invalida\n");
			continuar = 0;
		}
	}
	return continuar;
}

void agregarColaEspera(int id)
{
	colaEspera[id] = 1;
}

void eliminarColaEspera(int id)
{
	colaEspera[id] = 0;
}

void agregarColaEjecutados(int id)
{
	char buffer[MAXBUFFER];
	strcpy(buffer, "");
	sprintf(buffer, "%d", id);
	if (strcmp(colaEjecutados, "") == 0)
	{
		strcat(colaEjecutados, buffer);
	}else{
		strcat(colaEjecutados, ",");
		strcat(colaEjecutados, buffer);
	}		
}

void agregarColaMuertos(int id)
{
	printf("entre a cola muertos\n");
	char buffer[MAXBUFFER];
	strcpy(buffer, "");
	sprintf(buffer, "%d", id);
	
	if (strcmp(colaMuertos, "") == 0)
	{
		strcat(colaMuertos, buffer);
	}else{
		strcat(colaMuertos, ",");
		strcat(colaMuertos, buffer);
	}
}

unsigned int paginasDisponiblesEnMemoria()
{
	int cantidad = 0;
	int i;
	for (i = 0; i < maxMemoriaCompartida; i++)
	{
		if (memoria[i] == 0)
		{
			cantidad++;
		}
	}
	int resultado = cantidad / longitudPagina;
	return resultado;
}

int primerIndiceDisponible(int nEspacios)
{
	int i = 0;
	int j;
	int veces = nEspacios;
	while (i < maxMemoriaCompartida)
	{
		if (memoria[i] == 0)
		{
			j = i;
			while (veces > 0)
			{
				if (memoria[j] == 0)
				{
					j++;
					veces--;
				}else{
					break;
				}
			}
			if (veces == 0)
			{
				return i;
			}else{
				veces = nEspacios;
			}
		}
		i++;
	}
}
	
void asignarEspacioEnMemoriaPaginas(int id, int numPaginas)
{
	int desplazamiento;
	int codigo;
	int idPagina = 1;

	while (numPaginas > 0)
	{
		int inicio = primerIndiceDisponible(longitudPagina);
		for (desplazamiento = 0; desplazamiento < longitudPagina; desplazamiento++)
		{
			codigo = id * MASCARA;
			codigo = codigo + idPagina;
			memoria[inicio + desplazamiento] = codigo;
		}
		numPaginas--;
		idPagina++;
	}
}

void eliminarEspacioEnMemoriaPaginas(int id)
{
	int i;
	int codigo;
	for (i = 0; i < maxMemoriaCompartida; i++)
	{
		codigo = memoria[i];
		if (codigo / MASCARA == id){
			memoria[i] = 0;
		}
	}
}

void mostrarMemoriaAux()
{
	int i;
	for (i = 0; i < maxMemoriaCompartida; i++)
	{
		printf("%d %d\n", i, memoria[i]);
	}
}


void* buscarEspacioPaginacion(void* vargp)
{
	struct parametrosProcesoPagina *p;
	
	p = (struct parametrosProcesoPagina *) vargp;
	int id = p->id;
	int numPaginas = p->numPaginas;
	int tiempo = p->tiempo;
	
	printf("Proceso %d con %d paginas y %d tiempo creado\n", id, numPaginas, tiempo);
	
	fprintf(bitacora, "PID: %d esta esperando el semaforo %s\n", id, timestamp());
	agregarColaEspera(id);
	sem_wait(&semaforo);
	procesoBuscando = id;
	printf("Proceso %d adquiere el semaforo\n", id);
	fprintf(bitacora, "PID: %d consigue el semaforo %s\n", id, timestamp());
	eliminarColaEspera(id);
	
	printf("%d paginas disponibles en memoria\n", paginasDisponiblesEnMemoria());
	if (paginasDisponiblesEnMemoria() < numPaginas)
	{
		printf("El proceso %d muere lastimosamente por falta de espacio\n", id);
		fprintf(bitacora, "PID: %d no tuvo espacio suficiente, abortando solicitud %s\n", id, timestamp());
		agregarColaMuertos(id);
		procesoBuscando = 0;
		sem_post(&semaforo);
		pthread_exit(NULL);
		exit(1);
						
	}else{
			printf("P%d escribiendo en memoria\n", id);
			fprintf(bitacora, "PID: %d esta poniendo sus %d paginas en memoria %s\n", id, numPaginas, timestamp());
			asignarEspacioEnMemoriaPaginas(id, numPaginas);
			printf("P%d cediendo turno en memoria\n", id);
			fprintf(bitacora, "PID: %d ha liberado el semaforo %s\n", id, timestamp());
			procesoBuscando = 0;
			sem_post(&semaforo);

			printf("P%d ejecutando\n", id);
			fprintf(bitacora, "PID: %d ejecutando %s\n", id, timestamp());
			sleep(tiempo);
			
			agregarColaEspera(id);
			sem_wait(&semaforo);
			eliminarColaEspera(id);
			printf("P%d ha terminado ahora va a eliminar de memoria\n", id);
			fprintf(bitacora, "PID: %d ha terminado su ejecucion %s\n", id, timestamp());
			eliminarEspacioEnMemoriaPaginas(id);
			printf("El proceso %d ha eliminado sus paginas de la memoria\n", id);
			sem_post(&semaforo);
			agregarColaEjecutados(id);
			pthread_exit(NULL);
	}	
}

unsigned int cabenSegmentosMemoria(int nSegmentos, int nEspacios)
{
	if (maxMemoriaCompartida < nSegmentos * nEspacios)
	{
		return 0;
	}
	
	int i;
	int j;
	int veces = nEspacios;
	
	while (i < maxMemoriaCompartida)
	{
		if (memoria[i] == 0)
		{
			j = i;
			while (veces > 0)
			{
				if (memoria[j] != 0)
				{
					break;
				}else{
					j++;
					veces--;
				}
			}
			if (veces == 0)
			{
				nSegmentos--;
				if (nSegmentos == 0)
				{
					return 1;
				}
				veces = nEspacios;
			}
		}
		i++;
	}
	return 0;
}

void asignarEspacioEnMemoriaSegmentos(int id, int nSegmentos, int nEspacios)
{
	int i;
	int veces = nEspacios;
	int codigo;
	int numSeg = 1;
	while (nSegmentos > 0)
	{
		i = primerIndiceDisponible(nEspacios);
		//vamos a codificar el segmento
		codigo = id * MASCARA;
		codigo = codigo + numSeg;
		while (veces > 0)
		{
			memoria[i] = codigo;
			i++;
			veces--;
		}
		veces = nEspacios;
		nSegmentos--;
		numSeg++;
	}
}

void eliminarEspacioEnMemoriaSegmentos(int id)
{
	int i;
	int codigo;
	int idSeg;
	for (i = 0; i < maxMemoriaCompartida; i++)
	{
		codigo = memoria[i];
		idSeg = codigo / MASCARA;
		if (idSeg == id)
		{
			memoria[i] = 0;
		}
	}
}

void* buscarEspacioSegmentacion(void* vargp)
{
	struct parametrosProcesoSegmento *s;
	s = (struct parametrosProcesoSegmento *) vargp;
	int id = s->id;
	int nSegmentos = s->numSegmentos;
	int nEspaciosPorSegmento = s->espacioPorSegmento;
	int tiempo = s->tiempo;
	
	printf("Proceso %d con %d segmentos con %d espacios por segmento y tiempo %d creado\n",
	id, nSegmentos, nEspaciosPorSegmento, tiempo);
		
	fprintf(bitacora, "PID: %d esta esperando el semaforo %s\n", id, timestamp());
	agregarColaEspera(id);
	sem_wait(&semaforo);
	procesoBuscando = id;
	printf("Proceso %d adquiere el semaforo\n", id);
	fprintf(bitacora, "PID: %d consigue el semaforo %s\n", id, timestamp());
	eliminarColaEspera(id);
	
	
	if (!cabenSegmentosMemoria(nSegmentos, nEspaciosPorSegmento))
	{
		printf("El proceso %d muere lastimosamente por falta de espacio\n", id);
		fprintf(bitacora, "PID: %d no tuvo espacio suficiente, abortando solicitud %s\n", id, timestamp());
		procesoBuscando = 0;
		sem_post(&semaforo);
		agregarColaMuertos(id);
		pthread_exit(NULL);
		exit(1);
						
	}else{
			printf("P%d escribiendo en memoria\n", id);
			fprintf(bitacora, "PID: %d esta poniendo sus %d segmentos en memoria %s\n", id, nSegmentos, timestamp());
			asignarEspacioEnMemoriaSegmentos(id, nSegmentos, nEspaciosPorSegmento);
			printf("P%d cediendo turno en memoria\n", id);
			fprintf(bitacora, "PID: %d ha liberado el semaforo %s\n", id, timestamp());
			procesoBuscando = 0;
			sem_post(&semaforo);
				
			printf("P%d ejecutando\n", id);
			fprintf(bitacora, "PID: %d ejecutando %s\n", id, timestamp());
			sleep(tiempo);
			
			agregarColaEspera(id);
			sem_wait(&semaforo);
			eliminarColaEspera(id);
			printf("P%d ha terminado ahora va a eliminar de memoria\n", id);
			fprintf(bitacora, "PID: %d ha terminado su ejecucion %s\n", id, timestamp());
			eliminarEspacioEnMemoriaSegmentos(id);
			printf("El proceso %d ha eliminado sus segmentos de la memoria\n", id);
			sem_post(&semaforo);
			agregarColaEjecutados(id);
			pthread_exit(NULL);
	}	
}

void* crearProcesos(void* vargp)
{
	int idProceso = 1;
	
	int minTiempo = 20;
	int maxTiempo = 60;
	int minDormir = 30;
	int maxDormir = 60;
	
	int minPag = 1;
	int maxPag = 10;
	
	int minSeg = 1;
	int maxSeg = 5;
	int minEspacios = 1;
	int maxEspacios = 3;
	
	int tiempo;
	
	if (modoOperacionPagina)
	{
		while (continuar)
		{
			pthread_t thread1;
			int cantPag = minPag + rand() % (maxPag + 1 - minPag);
			int tiempo = minTiempo + rand() % (maxTiempo + 1 - minTiempo);
			
			struct parametrosProcesoPagina p;
			p.id = idProceso;
			idProceso = idProceso + 1;
			p.numPaginas = cantPag;
			p.tiempo = tiempo;
			
			pthread_create(&thread1, NULL, &buscarEspacioPaginacion, (void *) &p);
				
			int dormir = minDormir + rand() % (maxDormir + 1 - minDormir);
			sleep(dormir);
		}
		pthread_exit(NULL);
		
	}else{
		while (continuar)
		{
			pthread_t thread1;
			int cantSegmentos = minSeg + rand() % (maxSeg + 1 - minSeg);
			int cantEspacios = minEspacios + rand() % (maxEspacios + 1 - minEspacios);
			tiempo = minTiempo + rand() % (maxTiempo + 1 - minTiempo);
			
			struct parametrosProcesoSegmento s;
			s.id = idProceso;
			idProceso = idProceso + 1;
			s.numSegmentos = cantSegmentos;
			s.espacioPorSegmento = cantEspacios;
			s.tiempo = tiempo;
			
			pthread_create(&thread1, NULL, &buscarEspacioSegmentacion, (void *) &s);
			
			int dormir = minDormir + rand() % (maxDormir + 1 - minDormir);
			sleep(dormir);
		}
		pthread_exit(NULL);
	}
	return 0;
}

void mostrarMemoria()
{
	int i;
	int codigo;
	for (i = 0; i < maxMemoriaCompartida; i++)
	{
		if (modoOperacionPagina)
		{
			if (memoria[i] != 0)
			{
				codigo = memoria[i];
				printf("%d tomado por proceso %d pagina %d\n",
				   i, codigo / MASCARA, codigo % MASCARA);
				   
			}else{
				printf("%d libre\n", i);
			}
		}else{
			if (memoria[i] != 0)
			{
				int codigo = memoria[i];
				printf("%d tomado por proceso %d segmento %d\n",
				  i, codigo / MASCARA, codigo % MASCARA);
			}else{
				printf("%d libre\n", i);
			}
		}
	}
}

void pedirEstados()
{
	//se usa un vector para evitar repetir imprimir mismo ID
	int indices[MAXBUFFER] = { 0 };
	int i;
	int primeroEnPasar = 1;
	printf("PID de procesos en memoria: ");

	for (i = 0; i < maxMemoriaCompartida; i++)
	{
		int codigo = memoria[i] / MASCARA;
		if (indices[codigo] == 0 && codigo != 0)
		{
			if (primeroEnPasar)
			{
					printf("%d", codigo);
					primeroEnPasar = 0;
			}else{
					printf(",");
					printf("%d", codigo);
			}
			indices[codigo] = 1;
		}
	}
	printf("\n");
	
	primeroEnPasar = 1;
	
	if (procesoBuscando != 0)
	{
		printf("PID del proceso buscando espacio: %d\n", procesoBuscando);
	}else{
		printf("No hay proceso buscando espacio\n");
	}
	
	printf("Procesos bloqueados: ");
	int j;
	for (j = 0; j < MAXBUFFER; j++)
	{
		if (colaEspera[j] != 0)
		{
			if (primeroEnPasar)
			{
				printf("%d", j);
				primeroEnPasar = 0;
			}else{
				printf(",");
				printf("%d", j);
			}
		}
	}
	printf("\n");
	
	if (strlen(colaMuertos) > 0 && colaMuertos[0] != ',')
	{
		printf("Procesos que murieron por falta de espacio: %s\n", colaMuertos);
	}else{
		printf("No hay procesos que murieron por falta de espacio\n");
	}
	printf("Procesos que terminaron su ejecucion: %s\n", colaEjecutados);
}


int main()
{
	char entrada[100];
	if (inicializador()){
		printf("Inicializador completado\n");
		if (pedirModoOperacion()){
			bitacora = fopen("bitacora", "w");
			pthread_create(&hiloCreadorProcesos, NULL, crearProcesos, NULL);
		}
		while (continuar)
		{
			strcpy(entrada, "");
			scanf("%s", entrada);
			if (strcmp(entrada, "S") == 0)
			{
				continuar = 0;
				break;
			}else if(strcmp(entrada, "M") == 0)
			{
				mostrarMemoria();
			}else if (strcmp(entrada, "E") == 0)
			{
				pedirEstados();
			}
		}
		liberarSemaforo();
		liberarMemoria();
		fclose(bitacora);
		return 0;
	}
		
	return 0;
}
