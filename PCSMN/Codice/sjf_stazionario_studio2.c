#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include "rngs.h"

#define START         0.0
#define STOP     1000000.0
//#define STOP     5.0
#define INFINIT (100.0 * STOP)
#define A_tot 370000.0
#define NaP  (A_tot*0.2)
#define h 4.5
#define N 35
#define P 0.1031
#define l_tot 6.3




struct job {
    long id;
    int fog_id;
    double arrival_fog;
    double service_fog;
    double completion_fog;
    double service_cloud;
    double completion_cloud;
    int batch_departed_fog;
    int batch_departed_cloud;
    struct job *next;
};

struct job *job_list = NULL;       //istanti temporali per ogni job
long id = 0;                        //inizializza id del job
int batch_index = 0;

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



//lista coda

struct req {
    double size;
    struct req *next;
};

struct req *alloc_req(void)
{
    struct req *p;
    p = malloc(sizeof(struct req));
    if (p == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

void free_req(struct req *d)
{
    free(d);
}

void insert_after_req(struct req *new, struct req **pnext)
{
    new->next = *pnext;
    *pnext = new;
}

void remove_after_req(struct req **ppos)
{
    struct req *d = *ppos;
    *ppos = d->next;
    free_req(d);

}


void insert_sorted_list_req(struct req *new, struct req **pp)
{
    struct req *p;
    for (p = *pp; p != NULL; pp = &p->next, p = p->next)
        if (p->size > new->size) {
            insert_after_req(new, pp);
            return;
        }
    insert_after_req(new, pp);
}

void insert_req(double v, struct req **phead)
{
    struct req *new;

    new = alloc_req();
    new->size = v;
    insert_sorted_list_req(new, phead);
}



double Min(double a, double c)

{
    if (a < c)
        return (a);
    else
        return (c);
}


double Exponential(double m)
{
    return (-m * log(1.0 - Random()));
}


static double arrival = START;

double GetArrival()
{

    SelectStream(0);
    //double l_tot= (NaP/(3600.0*h));
    double interarr = 1.0/(l_tot);

    arrival += Exponential(interarr);

    return (arrival);
}


double GetService()
{
    SelectStream(1);
    return (Exponential(1/(0.8)));

}


double GetServiceCloud()
{
    SelectStream(3);
    return (Exponential(1/(0.65)));

}


double Uniform(double a, double b)

{
    SelectStream(5);
    return (a + (b - a) * Random());
}

struct {
    double arrival;
    double arrival_cloud;
    double current;
    double next;
    double last;
} t;

struct area {
    double node;
    double queue;
    double service;
};

struct fog_node{
    double completion;
    struct area nodo;
    long number;
    long index;
    struct req *req_head;
    double thr;
};


struct cloud_node{
    double completion;
    struct area nodo;
    long number;
    long index;
    struct req *req_head;
    double thr;
};

struct cloud_node cloud;
struct fog_node fog[N];


void simulation(double start, double stop, long seed, FILE *thf, FILE *thc, FILE *pqf, FILE*pqc)
{


    arrival = start;
    batch_index++;

    t.current    = start;
    t.arrival    = GetArrival();
    int i;                          //indice fog
    int finish = 0;

    while (!finish) {
        double minimo;
        minimo = fog[0].completion;
        int k = 0;
        int s;


        for (s = 0; s < N; s++) {
            if (minimo > fog[s].completion) {
                minimo = fog[s].completion;
                k = s;
            }
        }

        t.next = Min(Min(t.arrival, fog[k].completion), cloud.completion);

        if (t.next > stop) {
            finish = 1;
            continue;
        }


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


        t.current = t.next;

        if (t.current == t.arrival) {

            i = (int) Uniform(0.0, (N));


            fog[i].number++;
            insert_job(id, t.current, &job_list);
            job_list->fog_id = i;
            job_list->service_fog = GetService();
            id++;


            t.arrival = GetArrival();

            if (t.arrival > stop)  {
                t.last      = t.current;
                t.arrival   = INFINIT;
            }


            if (fog[i].number == 1){
                struct job *p;
                for (p = job_list; p != NULL; p = p->next)
                    if(p->arrival_fog == t.current)
                        break;
                fog[i].completion = t.current + p->service_fog;
                p->completion_fog = fog[i].completion;


            }else {
                insert_req(job_list->service_fog, &(fog[i].req_head));
            }


        }//completamento fog
        else if (t.current == fog[k].completion) {
            fog[k].index++;
            fog[k].thr = fog[k].index / (t.current - start);

            struct job *j;
            for (j = job_list; j != NULL; j = j->next)
                if(j->completion_fog == t.current)
                    break;
            j->batch_departed_fog = batch_index;

            fog[k].number--;

            double p = Uniform(0.0, 1.0);

            if (p > P) {
                // risposta all'utente
            } else {

                cloud.number++;

                for (j = job_list; j != NULL; j = j->next)
                    if(j->completion_fog == t.current)
                        break;
                j->service_cloud = GetServiceCloud();

                if (cloud.number == 1) {

                    cloud.completion = t.current + j->service_cloud;
                    j->completion_cloud = cloud.completion;

                } else {
                    insert_req(j->service_cloud, &(cloud.req_head));
                }
            }
            if (fog[k].number > 0) {

                for (j = job_list; j != NULL; j = j->next)
                    if(j->service_fog == fog[k].req_head->size)
                        break;
                fog[k].completion = t.current + fog[k].req_head->size;
                j->completion_fog = fog[k].completion;

                remove_after_req(&(fog[k].req_head));

            }else {

                fog[k].completion = INFINIT;

            }



        } //completamento cloud
        else{
            cloud.index++;

            cloud.thr = cloud.index / (t.current - start);
            cloud.number--;

            struct job *j;
            for (j = job_list; j != NULL; j = j->next)
                if(j->completion_cloud == t.current)
                    break;
            j->batch_departed_cloud = batch_index;

            if(cloud.number > 0){
                struct job *p;
                for (p = job_list; p != NULL; p = p->next)
                    if(p->service_cloud == cloud.req_head->size)
                        break;
                cloud.completion = t.current + cloud.req_head->size;
                p->completion_cloud = cloud.completion;

                remove_after_req(&(cloud.req_head));
            }
            else{
                cloud.completion=INFINIT;
            }
        }

    }


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

    /*Per lo stazionario il seed è comune ad ogni batch */

    long seed = 123456789;
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
        fog[d].req_head = NULL;
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
    cloud.req_head = NULL;
    cloud.thr = 0.0;
    t.arrival_cloud = INFINIT;

    /* ----------------------------
     * File output per statistiche
     * ----------------------------
     * */
    for(int i = 1 ; i <= k; i++){

      
         /* -----------------------------------------
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

