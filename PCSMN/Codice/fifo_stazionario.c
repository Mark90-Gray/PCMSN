#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include "rngs.h"

#define START         0.0                       //istante di inizio simulazione
#define STOP     100000.0                       //istante di "close the door"
#define INFINIT (100.0 * STOP)                  //valore utilizzato come istante infinito
#define A_tot 370000.0                          //numero di auto totali circolanti
#define NaP  (A_tot*0.2)                        //numero di auto in cerca di parcheggio
#define h 4.5                                   //totale ore di punta considerate
#define N 35                                    //numero di nodi fog nel sistema
#define P 0.1031                                // probabilità di routing da aggiornare al variare del numero di fog N
// #define l_tot 1                              // tasso di arrivo per studio 2


/* Per analisi stazionario studio 1: 
* N: {6, 10, 30, 35, 40, 50, 75, 100, 120, 150, 190}
* P: {0.0913, 0.093406, 0.1020, 0.1031, 0.1056, 0.1090, 0.1164, 0.1228, 0.1275, 0.1338, 0.140964 }
* Per analisi stazionario studio 2:
* l_tot:{1, 2, 3, 4, 5, 6, 6.3}
* nota: in get arrival utilizzare tasso di arrivo per analisi studio 2
*/


/*
 * Struttua dati che raccoglie tutte le informazioni relative ad una richiesta, in particolare
 * gli istanti di tempo in cui si verifica un evento significativo che la riguarda
 * */

struct job {
    long id;                        //identificativo univoco della richiesta
    int fog_id;                     //id del nodo che la soddisfa
    double arrival_fog;             //istante di arrivo al fog
    double service_fog;             //istante in cui è posta in servizio sul fog
    double completion_fog;          //istante di completamento sul fog che coincide con l'istante di arrivo sul
                                    //cloud (tempi di trasmissione inclusi nei tempi di servizio)
    double service_cloud;           //istante in cui è posta in servizio sul cloud
    double completion_cloud;        //istante di completamento sul cloud
    int batch_departed_fog;          //per tener traccia dei completamenti del fog in un determinato batch
    int batch_departed_cloud;        //per tener traccia dei completamenti del cloud in un determinato batch
    struct job *next;                //puntatore alla struttura dati della richiesta successiva
};

struct job *job_list = NULL;        //dichiarazione della struttura dati globale

long id = 0;                        //id della prima richiesta

int batch_index = 0;                // id del primo batch 



/*
 * Funzioni per la gestione della lista job
 * */

struct job *alloc_job(void)
{
    struct job *p;
    p = malloc(sizeof(struct job));
    if (p == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }
    p->arrival_fog = START;
    p->service_fog = START;
    p->completion_fog = INFINIT;
    p->service_cloud = START;
    p->completion_cloud = INFINIT;
    p->batch_departed_fog = 0;                
    p->batch_departed_cloud = 0;
    return p;
}

void free_job(struct job *d)
{
    free(d);
}

void insert_after_job(struct job *new, struct job **pnext)
{
    new->next = *pnext;
    *pnext = new;
}

void remove_after_job(struct job **ppos)
{
    struct job *d = *ppos;
    *ppos = d->next;
    free_job(d);

}


void insert_sorted_job(struct job *new, struct job **pp)
{
    struct job *p;
    for (p = *pp; p != NULL; pp = &p->next, p = p->next)
        if (p->id < new->id) {
            insert_after_job(new, pp);
            return;
        }
    insert_after_job(new, pp);
}

void insert_job(long index, double arrival, struct job **phead)
{
    struct job *new;

    new = alloc_job();
    new->id = index;
    new->arrival_fog = arrival;
    insert_sorted_job(new, phead);
}

/*
 *Struttura dati per simulare il comportamentoo di una coda infinita utillizzata sia per
 * i nodi fog sia per il cloud
 * */

struct queue_req {
    double arrival;                 //indica l'istante di arrivo in coda
    struct queue_req *next;         //puntatore alla prossima richiesta in coda
};

/*
 * Funzioni per la gestione delle code
 * */


struct queue_req *alloc_req(void)
{
    struct queue_req *p;
    p = malloc(sizeof(struct queue_req));
    if (p == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

void free_req(struct queue_req *d)
{
    free(d);
}

void insert_after_req(struct queue_req *new, struct queue_req **pnext)
{
    new->next = *pnext;
    *pnext = new;
}

void remove_after_req(struct queue_req **ppos)
{
    struct queue_req *d = *ppos;
    *ppos = d->next;
    free_req(d);

}


void insert_sorted_queue(struct queue_req *new, struct queue_req **pp)
{
    struct queue_req *p;
    for (p = *pp; p != NULL; pp = &p->next, p = p->next)
        if (p->arrival > new->arrival) {
            insert_after_req(new, pp);
            return;
        }
    insert_after_req(new, pp);
}

void insert_req(double v, struct queue_req **phead)
{
    struct queue_req *new;

    new = alloc_req();
    new->arrival = v;
    insert_sorted_queue(new, phead);
}

/* ------------------------------
 * return the smaller of a, b
 * ------------------------------
 */

double Min(double a, double c)

{
    if (a < c)
        return (a);
    else
        return (c);
}

/* ---------------------------------------------------
 * generate an Exponential random variate, use m > 0.0
 * ---------------------------------------------------
 */

double Exponential(double m)
{
    return (-m * log(1.0 - Random()));
}

//variabile globale che indica l'istante di partenza degli arrivi
static double arrival = START;

/* ---------------------------------------------
 * generate the next arrival time
 * ---------------------------------------------
 */

double GetArrival()
{

    //seleziono uno specifico stream
    SelectStream(0);
    
    double l_tot= (NaP/(3600.0*h));     //non utilizzare in caso di studio 2
    double interarr = 1.0/(l_tot);

    arrival += Exponential(interarr);

    return (arrival);
}

/*
 * Genera un tempo di completamento per le richieste sul nodo fog
 * */
double GetService()
{
    //seleziono uno specifico stream
    SelectStream(1);
    return (Exponential(1/(0.8)));

}

/*
 * Genera un tempo di completamento per le richieste sul cloud
 * */
double GetServiceCloud()
{
    //seleziono uno specifico stream
    SelectStream(3);
    return (Exponential(1/(0.65)));

}

/* --------------------------------------------
 * generate a Uniform random variate, use a < b
 * --------------------------------------------
 */
double Uniform(double a, double b)

{
    //seleziono uno specifico stream
    SelectStream(5);
    return (a + (b - a) * Random());
}

    /*
     * Struttura dati che scandisce l'avanzare del clock di sistema
     * */
   struct {
        double arrival;         //istante dell'arrivo più recente
        double current;         //istante dell'evento corrente in gestione
        double next;            //istante del prossimo evento da gestire
        double last;            //istante dell'ultimo arrivo della simulazione
    } t;

    /*
     * Struttura dati per calcolare la somma integrale nel tempo delle richieste nel sistema
     * */
    struct area {
        double node;                    /* time integrated number in the node  */
        double queue;                   /* time integrated number in the queue */
        double service;                 /* time integrated number in service   */
    };

   /*
     * Struttura dati che contiene le informazioni relative al singolo nodo fog
     * */
    struct fog_node{
        double completion;              //istante di completamento della richiesta in gestione
        struct area nodo;               //valori integrali relativi al singolo nodo fog
        long number;                    //numero di righieste attualmente nel nodo
        long index;                     //numero di completamenti
        struct queue_req *coda;         //coda del nodo fog
        double thr;                     //throughput relativo al singolo nodo fog
    };



    /*
     * Struttura dati che contiene le informazioni relative al cloud
     * */
    struct cloud_node{
        double completion;              //istante di completamento della richiesta in gestione
        struct area nodo;               //valori integrali relativi al cloud
        long number;                    //numero di righieste attualmente nel cloud
        long index;                     //numero di completamenti
        struct queue_req *coda;         //coda del cloud
        double thr;                     //throughput relativo al cloud
    };



     struct cloud_node cloud;            //struttura dati cloud
     struct fog_node fog[N];             //struttura dati fog

/*
 * Funzione che rappresenta il cuore della simulazione next-event, dove viene gestito ogni evento e fatto avanzare il
 * clock all'occorrenza di ogni nuovo evento.
 * @ start rappresenta l'istante di inizio della simulazione
 * @ stop rappresenta l'istante di fine della simulazione
 * @ seed rappresenta il seed impostato per la simulazione corrente
 * @ thf e thc indicano i file dove scrivere i valori di throughput calcolati
 * @ pqf e pqc indicano i file dove scrivere i valori di popolazione media in coda calcolati
 * */
void simulation(double start, double stop, long seed, FILE *thf, FILE *thc, FILE *pqf, FILE*pqc)
{

    arrival = start;
    batch_index++;

    t.current    = start;
    t.arrival    = GetArrival();
    int i;                          //indice fog
    int finish = 0;                 // finish = 1 indica la fine di un batch

    /*
     * Ciclo principale per la gestione degli eventi
     * */

    while (!finish) {
        double minimo;
        minimo = fog[0].completion;
        int k = 0;
        int s;

         //individuo l'evento di completamento più prossimo e il nodo fog in cui si verifica
        for (s = 0; s < N; s++) {
            if (minimo > fog[s].completion) {
                minimo = fog[s].completion;
                k = s;
            }
        }

        //seleziono il prossimo evento da gestire
        t.next = Min(Min(t.arrival, fog[k].completion), cloud.completion);

        /*controllo se l'istante dell'evento successivo è maggiore dello stop, nel caso positivo esco dal ciclo e
        * finish viene impostato ad 1, il batch è finito.
        **/
        if (t.next > stop) {
            finish = 1;
            continue;
        }

        //aggiorno le strutture dati per gli integrali dei nodi fog e del cloud
        for (int j = 0; j < N; j++) {
            if (fog[j].number > 0) {
                fog[j].nodo.node += (t.next - t.current) * fog[j].number;
                fog[j].nodo.queue += (t.next - t.current) * (fog[j].number - 1);
                fog[j].nodo.service += (t.next - t.current);
            }


        }

        if (cloud.number > 0) {

            cloud.nodo.node += (t.next - t.current) * cloud.number;
            cloud.nodo.queue += (t.next - t.current) * (cloud.number - 1);
            cloud.nodo.service += (t.next - t.current);

        }

        //faccio avanzare il clock
        t.current = t.next;

        //l'evento da gestire corrisponde ad un arrivo
        if (t.current == t.arrival) {

            //seleziono in modo uniforme su quale nodo fog far gestire l'arrivo
            i = (int) Uniform(0.0, (N));
            fog[i].number++;

            //inserisco la nuova richiesta nella lista dei job totali
            insert_job(id, t.current, &job_list);
            job_list->fog_id = i;
            id++;

             //genero il prossimo arrivo
            t.arrival = GetArrival();

            //se l'arrivo generato è oltre la soglia di stop aggiorna il clock
            if (t.arrival > stop)  {
                t.last      = t.current;
                t.arrival   = INFINIT;
            }
            //se il nodo fog è idle metti in servizio la richiesta
            if (fog[i].number == 1){
                //individuo nella lista di job quella relativa alla richiesta corrente e la aggiorno
                struct job *p;
                for (p = job_list; p != NULL; p = p->next)
                    if(p->arrival_fog == t.current)
                        break;
                //genero il tempo di servizio
                p->service_fog = GetService();
                //salvo l'istante di completamento
                fog[i].completion = t.current + p->service_fog;
                p->completion_fog = fog[i].completion;


            }else {
                //se il nodo non è idle inserisci la richiesta in coda
                insert_req(t.current, &(fog[i].coda));
            }




        }//l'evento da gestire corrisponde ad un completamento su un fog
        else if (t.current == fog[k].completion) {
            //aggiorno il valore del throughput
            fog[k].index++;
            //individuo nella lista di job quella relativa alla richiesta corrente e la aggiorno
            struct job *j;
            for (j = job_list; j != NULL; j = j->next)
                if(j->completion_fog == t.current)
                    break;
                //salvo il riferimento al batch dove è avvenuto il completamento
            j->batch_departed_fog = batch_index;
                //aggiornamento del throughput
            fog[k].thr = fog[k].index / (t.current - start);
                // aggiornamento popolazione nel sistema
            fog[k].number--;

             /*
             * Genero la probabilità che la richiesta trovi il parcheggio pieno e venga reindirizzata al cloud
             * */
            double p = Uniform(0.0, 1.0);

            if (p > P) {
                //parcheggio libero: viene inviata una risposta all'utente
            } else {
                //parcheggio non libero: la richiesta è inviata al cloud
                cloud.number++;

                 //se il cloud è idle metti in servizio la richiesta
                if (cloud.number == 1) {
                    //individuo nella lista di job quella relativa alla richiesta corrente e la aggiorno
                    for (j = job_list; j != NULL; j = j->next)
                        if(j->completion_fog == t.current)
                            break;
                    //genero il tempo di servizio
                    j->service_cloud = GetServiceCloud();
                     //salvo l'istante di completamento
                    cloud.completion = t.current + j->service_cloud;
                    j->completion_cloud = cloud.completion;

                }else {
                     //se il cloud non è idle inserisci la richiesta in coda
                    insert_req(t.current, &(cloud.coda));
                }
            }//dopo aver soddisfato una richiesta il nodo fog controlla la sua coda e se non è vuota la serve
            if (fog[k].number > 0) {
                //individuo nella lista di job quella relativa alla richiesta corrente e la aggiorno
                for (j = job_list; j != NULL; j = j->next)
                    if(j->arrival_fog == fog[k].coda->arrival)
                        break;
                //genero il tempo di servizio
                j->service_fog = GetService();
                //salvo l'istante di completamento
                fog[k].completion = t.current + j->service_fog;
                j->completion_fog = fog[k].completion;

                 //rimuovo la richiesta dalla coda
                remove_after_req(&(fog[k].coda));

            }else {
                //non ci sono richieste in coda
                fog[k].completion = INFINIT;

            }



        }//l'evento da gestire corrisponde ad un completamento sul cloud
        else{
             //aggiorno il valore del throughput
            cloud.index++;
            cloud.thr = cloud.index / (t.current - start);
            cloud.number--;
           
            struct job *j;
            // individuo nella lista di job la richiesta corrente e la aggiorno
            for (j = job_list; j != NULL; j = j->next)
                if(j->completion_cloud == t.current)
                    break;
            //salvo il riferimento al batch dove è avvenuto il completamento del cloud
            j->batch_departed_cloud = batch_index;

             //dopo aver soddisfato una richiesta il cloud controlla la sua coda e se non è vuota la serve
            if(cloud.number > 0){
                //individuo nella lista di job quella relativa alla richiesta corrente e la aggiorno
                struct job *p;
                for (p = job_list; p != NULL; p = p->next)
                    if(p->completion_fog == cloud.coda->arrival)
                        break;
                //genero il tempo di servizio
                p->service_cloud = GetServiceCloud();
                //salvo l'istante di completamento
                cloud.completion = t.current + p->service_cloud;
                p->completion_cloud = cloud.completion;
                //rimuovo la richiesta dalla coda
                remove_after_req(&(cloud.coda));

            }
            else{
                //non ci sono richieste in coda
                cloud.completion=INFINIT;
            }
        }

    }

    // aggiorno la struttura dati per gli integrali dei nodi e fog e cloud 
    for (int j = 0; j < N; j++) {
        if (fog[j].number > 0) {
            fog[j].nodo.node += (stop - t.current) * fog[j].number;
            fog[j].nodo.queue += (stop - t.current) * (fog[j].number - 1);
            fog[j].nodo.service += (stop - t.current);
        }


    }

    if (cloud.number > 0) {

        cloud.nodo.node += (stop - t.current) * cloud.number;
        cloud.nodo.queue += (stop - t.current) * (cloud.number - 1);
        cloud.nodo.service += (stop - t.current);

    }

    //scrivo su file i valori di throughput e popolazione calcolati
    fprintf(thf, "%f\n", fog[0].thr);
    fprintf(pqf, "%f\n", fog[0].nodo.queue / (stop - start));

    fprintf(thc, "%f\n", cloud.thr);
    fprintf(pqc, "%f\n", cloud.nodo.queue / (stop - start));


}




/* --------------------------------------------------------------------------------------------
 * Main:
 * il seguente main è stato implementato per archiviare i risultati statistici in più directory 
 * e in più file nel formato da usare per i programmi uvs.c ed estimate.c.
 * --------------------------------------------------------------------------------------------
 * */

int main(void)
{

    /* -------------------------------------------------------------------------------------------
     *Inizializzazione variabili simulazione:
    * fog_select è stata definita al fine di poter creare file per le statistiche di più nodi fog
    * negli studi condotti si fa riferimento solo al fog 0 in quanto tutti i nodi fog si comportano
    * nello stesso modo.
    * nume_seed definisce il numero di seed utilizzati, per lo studio dello stazionario il valore è 
    * posto ad 1.
    *----------------------------------------------------------------------------------------------
    * */
    int fog_select =  1; 
    int num_seed = 1;
    double simulation_time = 10000000.0;
    int k = 64;
    double start = 0.0;

    long seed = 123456789;

    /*Per lo stazionario il seed è comune ad ogni batch */
    PlantSeeds(seed);

    /* ----------------------------------
    * FOG inizializzazione struttura dati
    * -----------------------------------
    * */
    for(int d=0; d<N; d++){
        fog[d].completion = INFINIT;
        fog[d].number=0;
        fog[d].index=0;
        fog[d].nodo.node=0.0;
        fog[d].nodo.queue=0.0;
        fog[d].nodo.service=0.0;
        fog[d].coda = NULL;
        fog[d].thr = 0.0;

    }
     /* -----------------------------------
    * CLOUD inizializzazione struttura dati
    * ------------------------------------
    *  */
    cloud.completion = INFINIT;
    cloud.number = 0;
    cloud.index = 0;
    cloud.nodo.node = 0.0;
    cloud.nodo.queue = 0.0;
    cloud.nodo.service = 0.0;
    cloud.coda = NULL;
    cloud.thr = 0.0;
    t.arrival_cloud = INFINIT;

     /* ----------------------------
     * File output per statistiche
     * ----------------------------
     * */
    for(int i = 1 ; i <= k; i++){

        / /* -----------------------------------------
         * Creazione directory output per ogni batch
         * -----------------------------------------
         * */
        char directory[20] = "Service";
        char directoryR[20] = "Response";
        char directoryT[20] = "Throughput";
        char directoryPQ[20] = "PopulationQueue";
        char directoryTQ[20] = "TimeQueue";
        char num[10];
        snprintf(num, 11, "%d", i);
        strcat(directory, num);
        strcat(directoryR, num);
        strcat(directoryT, num);
        strcat(directoryPQ, num);
        strcat(directoryTQ, num);

        struct stat dir = {0};
        if(stat(directory, &dir) == -1)
        {
            mkdir(directory);
            //printf("created directory testdir successfully! \n");
        }
        if(stat(directoryR, &dir) == -1)
        {
            mkdir(directoryR);
            //printf("created directory testdir successfully! \n");
        }

        if(stat(directoryT, &dir) == -1)
        {
            mkdir(directoryT);
            //printf("created directory testdir successfully! \n");
        }
        if(stat(directoryPQ, &dir) == -1)
        {
            mkdir(directoryPQ);
            //printf("created directory testdir successfully! \n");
        }
        if(stat(directoryTQ, &dir) == -1)
        {
            mkdir(directoryTQ);
            //printf("created directory testdir successfully! \n");
        }
        char slash[3]="/";
        strcat(directory, slash);
        strcat(directoryR, slash);
        strcat(directoryT, slash);
        strcat(directoryPQ, slash);
        strcat(directoryTQ, slash);

       
        char directoryT_T[20];
        char directoryPQ_PQ[20];
        char directoryCT_T[20];
        char directoryCPQ_PQ[20];
        strcpy(directoryT_T, directoryT);
        strcpy(directoryPQ_PQ, directoryPQ);
        strcpy(directoryCT_T, directoryT);
        strcpy(directoryCPQ_PQ, directoryPQ);
        char fileName4[20] = "fog";
        char fileName5[20] = "cloud";
        char num2[3];
        snprintf(num2, 11, "%d",i);
        strcat(fileName4, num2);
        strcat(fileName5, num2);
        strcat(directoryT_T, fileName4);
        strcat(directoryPQ_PQ, fileName4);
        strcat(directoryCT_T, fileName5);
        strcat(directoryCPQ_PQ, fileName5);
        FILE *th; //throughput fog node 0
        FILE *pq; //poplazione in coda fog node 0
        FILE *thc; // throughput  cloud
        FILE *pqc; //popolazione in coda cloud

        th = fopen(directoryT_T, "w");
        if (th == NULL) return -1;
        pq = fopen(directoryPQ_PQ, "w");
        if (pq == NULL) return -1;
        thc = fopen(directoryCT_T, "w");
        if (thc == NULL) return -1;
        pqc = fopen(directoryCPQ_PQ, "w");
        if (pqc == NULL) return -1;

        for(int j = 0; j < num_seed; j++){

             /* ----------------------
             * Creazione file output per ogni seed
             * nello stazionario  il num_seed=1
             * ----------------------
             * */
            FILE *f[fog_select]; //time service
            FILE *g[fog_select]; //time response
            FILE *tq[fog_select]; //time queue

            for(int l=0; l<fog_select; l++){
                 /* ------------------------------------------------
                 * Creazione file per ogni fog node selezionato
                 * c'è omogeneità dei fog node fog_select=1
                 *-------------------------------------------------
                 * */
                char directory_S[30] ;
                char directory_R[30] ;
                char directory_T[30];
                char directory_PQ[30];
                char directory_TQ[30];
                strcpy(directory_S, directory);
                strcpy(directory_R, directoryR);
                strcpy(directory_T, directoryT);
                strcpy(directory_PQ, directoryPQ);
                strcpy(directory_TQ, directoryTQ);

                char fileName3[20] = "fog";



                char num3[10];
                snprintf(num3, 11, "%d_%d", l , j);

                strcat(fileName3, num3);

                strcat(directory_S, fileName3);
                strcat(directory_R, fileName3);
                strcat(directory_TQ, fileName3);

                g[l] = fopen(directory_R, "w");
                if (g[l] == NULL) return -1;
                f[l] = fopen(directory_S, "w");
                if (f[l] == NULL) return -1;
                tq[l] = fopen(directory_TQ, "w");
                if (tq[l] == NULL) return -1;


            }

             /* --------------------------------------------------------
             * Inizializzazione directory e file per CLOUD
             *-----------------------------------------------------------
             * */
            char directoryTS_TS[20];
            char directoryR_R[20];
            char directoryTQ_TQ[20];


            strcpy(directoryTS_TS, directory);
            strcpy(directoryR_R, directoryR);
            strcpy(directoryTQ_TQ, directoryTQ);


            char fileName2[10] = "cloud";
            char num3[10];
            snprintf(num3, 11, "_%d" , j);
            strcat(fileName2, num3);

            strcat(directoryTS_TS, fileName2);
            strcat(directoryR_R, fileName2);
            strcat(directoryTQ_TQ, fileName2);

            /* --------------------------------------------------------
             * Creazione file cloud
             * c: tempi di servizio,
             * cr:tempi di risposta,
             * ctq:tempi medi in coda
             *-----------------------------------------------------------
             * */
            FILE *c = fopen(directoryTS_TS, "w"); //service cloud
            if (c == NULL) return -1;
            FILE *cr = fopen(directoryR_R, "w"); //response cloud
            if (cr == NULL) return -1;
            FILE *ctq = fopen(directoryTQ_TQ, "w"); //response cloud
            if (ctq == NULL) return -1;



        
            /* ---------------------------------------------------------------------
             * Simulazione:
             * start aggiorna il tempo di inizio simulazione di ogni batch (= al tempo di stop del batch precedente)
             * (start + simulation_time/k) aggiorna lo stop per la simulazione di ogni batch 
             * seed imposta il seed della simulazione i-esima             
             * th: file throughput fog[0], 
             * tch: file throughput cloud,
             * pq:file popolazione media in coda fog[0],
             * pqc: file popolazione media in cloud.
             *----------------------------------------------------------------------
            */

            simulation(start, (start + simulation_time/k), seed, th, thc, pq, pqc);

             /* --------------------------------------------------------
             * Riempimento dei file corrispettivi per fog[0] e per cloud
             *-----------------------------------------------------------
             * */
            struct job *p;
            for (p = job_list; p != NULL; p = p->next) {
                if(p->fog_id == 0){//prendere solo i valori relativi al fog 0
                    if(p->batch_departed_fog == i){

                        fprintf(f[p->fog_id], "%f\n", p->service_fog);
                        fprintf(g[p->fog_id], "%f\n", (p->completion_fog - p->arrival_fog));
                        if((p->completion_fog - p->arrival_fog) - p->service_fog > 0.00000) {
                            fprintf(tq[p->fog_id], "%f\n", (p->completion_fog - p->arrival_fog) - p->service_fog);
                        }else {
                            fprintf(tq[p->fog_id], "%f\n", 0.0);
                        }
                    }
                }

                if(p->batch_departed_cloud == i) {
                    fprintf(c, "%f\n", p->service_cloud);
                    fprintf(cr, "%f\n", (p->completion_cloud - p->completion_fog));
                    if((p->completion_cloud - p->completion_fog) - p->service_cloud > 0.000000) {
                        fprintf(ctq, "%f\n", (p->completion_cloud - p->completion_fog) - p->service_cloud);
                    }else {
                        fprintf(ctq, "%f\n", 0.0);
                    }
                }
            }


           /* --------------------------------------------------------
             * Chiusura file
             *-----------------------------------------------------------
             * */

            for(int z=0; z < fog_select; z++) {
                fclose(f[z]);
                fclose(g[z]);
                fclose(tq[z]);

            }
            fclose(ctq);
            fclose(c);
            fclose(cr);


        }
        fclose(pqc);
        fclose(thc);
        fclose(pq);
        fclose(th);
        printf("Finished batch  %d su 64 \n", i);

         /* --------------------------------------------------------
         * Aggiornamento start per nuovo batch
         *-----------------------------------------------------------
         * */


        start += simulation_time/k; 


        /* --------------------------------------------------------
         * Rinizializzazione strutture per nuovo batch
         *-----------------------------------------------------------
         * */

        for(int d=0; d<N; d++){
            fog[d].index=0;
            fog[d].nodo.node=0.0;
            fog[d].nodo.queue=0.0;
            fog[d].nodo.service=0.0;
        }

        cloud.index = 0;
        cloud.nodo.node = 0.0;
        cloud.nodo.queue = 0.0;
        cloud.nodo.service = 0.0;

    }

     /* --------------------------------------------------------
     * Svuotamento job_list
     *-----------------------------------------------------------
     * */
    while (job_list != NULL) {
        remove_after_job(&job_list);
    }



    return 0;
}


