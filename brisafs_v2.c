/*
 * Emilio Francesquini <e.francesquini@ufabc.edu.br>
 * 2019-02-03
 *
 * Este código foi criado como parte do enunciado do projeto de
 * programação da disciplina de Sistemas Operacionais na Universidade
 * Federal do ABC. Você pode reutilizar este código livremente
 * (inclusive para fins comerciais) desde que sejam mantidos, além
 * deste aviso, os créditos aos autores e instituições.

 * Licença: CC-BY-SA 4.0
 *
 * O código abaixo implementa (parcialmente) as bases sobre as quais
 * você deve construir o seu projeto. Ele provê um sistema de arquivos
 * em memória baseado em FUSE. Parte do conjunto minimal de funções
 * para o correto funcionamento do sistema de arquivos está
 * implementada, contudo nem todos os seus aspectos foram tratados
 * como, por exemplo, datas, persistência e exclusão de arquivos.
 *
 * Em seu projeto você precisará tratar exceções, persistir os dados
 * em um único arquivo e aumentar os limites do sistema de arquivos
 * que, para o código abaixo, são excessivamente baixos (ex. número
 * total de arquivos).
 *
 */

/*
    Nota 5
        Persistência - Incluindo a criação e "formatação" de um arquivo novo para conter o seu "disco".     --FEITO
            Veja a função ftruncate para criar um arquivo com o tamanho pré-determinado                     --Não usamos está função, criamos nos metodos de read e write.
        Armazenamento e recuperação de datas (via ls por exemplo)       --FEITO
        Armazenamento e alteração direitos usando chown e chgrp         --FEITO
        Aumento do número máximo de arquivos para pelo menos 1024       --FEITO

    Nota 7
        Aumento do tamanho máximo do arquivo para pelo menos 64 MB      --FEITO
        Suporte à criação de diretórios                                 --Não iniciado
        Exclusão de arquivos                                            --FEITO

    Nota 10
        Suporte a "discos" de tamanhos arbitrários                      --FEITO, utilizamos a RAM como disco, e só usamos o DISCO para persistencia.
        Arquivos com tamanho máximo de pelo menos 1GB                   --FEITO
        Controle de arquivos abertos/fechados                           --Não Iniciado

    Nota 12
        Funcionalidades julgadas excepcionais além das pedidas podem gerar um bônus de até 2 pontos. 
        Converse com o professor para saber se a sua ideia é considerada excepcional e quanto ela vale.

*/

#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
/* Inclui a bibliteca fuse, base para o funcionamento do nosso FS */
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#define TAM_BLOCO 4096
/* A atual implementação utiliza apenas um bloco para todos os inodes
   de todos os arquivos do sistema. Ou seja, cria um limite rígido no
   número de arquivos e tamanho do dispositivo. - Esta parte foi desmontada */
#define MAX_FILES (int)floor(memoria_disponivel*QUANT_INODE)
#define QUANT_INODE 0.01
/* Para o superbloco e o resto para os arquivos. Os arquivos nesta
   implementação também tem uma quantidade de bloco, para conseguir guardar aquivos maiores que 1 G
   Se cada arquivo puder usar mais de 1 bloco. */
#define MAX_BLOCOS (memoria_disponivel + quant_blocos_superinode)
/* Parte da sua tarefa será armazenar e recuperar corretamente os
   direitos dos arquivos criados */
#define DIREITOS_PADRAO 0644
/*guarda a quantia de blocos que o superbloco precisa para o requsito de 2048 arquivos*/
#define quant_blocos_superinode ((MAX_FILES/(TAM_BLOCO/sizeof(inode)))+1)
typedef char byte;

/* Um inode guarda todas as informações relativas a um arquivo como
   por exemplo nome, direitos, tamanho, bloco inicial, ... */
typedef struct {
    char nome[250];
    uint16_t direitos;
    long int tamanho;
    uid_t usuario;
    gid_t grupo;
    time_t last_access;
    time_t last_mod;
    uint16_t bloco;
    int quant_blocos;
} inode;

/* Disco - A variável abaixo representa um disco que pode ser acessado
   por blocos de tamanho TAM_BLOCO com um total de MAX_BLOCOS. Você
   deve substituir por um arquivo real e assim persistir os seus
   dados! */
byte *disco;
FILE *Tmp_disco;

//guarda os inodes dos arquivos
inode *superbloco;
//Saber onde estou gravando neste momento
int gravacao_bloco_conteudo;
//Memoria disponivel para usar no SO (25% da memoria ram do computador)
int memoria_disponivel;

#define DISCO_OFFSET(B) (B * TAM_BLOCO)

//utilizada para escrever um arquivo binario nocomputador, usado para a persistencia
void persistecia_write(){
    Tmp_disco = fopen("Persistencia","wb");
    fwrite(disco,1,(gravacao_bloco_conteudo+2)*TAM_BLOCO,Tmp_disco);
    fflush(stdout);
    fclose(Tmp_disco);    
}

//le o conteudo caso ele exista da ultima vez que o SO rodou, caso positivo, carrega na memoria RAM que já foi reservada, garantindo a persistencia
void persistecia_read(){
    disco = calloc (MAX_BLOCOS, TAM_BLOCO);
    Tmp_disco = fopen("Persistencia","rb");
    if(Tmp_disco != NULL){
        long posInicial = ftell(Tmp_disco);
        fseek(Tmp_disco,0,SEEK_END);
        long tamanho = ftell(Tmp_disco);
        fseek(Tmp_disco,posInicial,SEEK_SET);
        if(fread(disco,1,tamanho,Tmp_disco)!=0);
        fflush(stdin);
        fclose(Tmp_disco);
    }
}

/* Preenche os campos do superbloco de índice isuperbloco */
void preenche_bloco (int isuperbloco, const char *nome, uint16_t direitos,
                     uint16_t tamanho, uint16_t bloco, const byte *conteudo) {
                         
    char *mnome = (char*)nome;

    //Joga fora a(s) barras iniciais
    while (mnome[0] != '\0' && mnome[0] == '/')
        mnome++;

    strcpy(superbloco[isuperbloco].nome, mnome);
    superbloco[isuperbloco].direitos = direitos;
    superbloco[isuperbloco].tamanho = tamanho + superbloco[isuperbloco].tamanho;
    superbloco[isuperbloco].bloco = bloco;
    //Iniciar com as datas preenchidas
    //conforme usa a funcao utimens_brisafs cuidas das datas, esta funcao já foi implementada pelo grupo
    superbloco[isuperbloco].last_access = time(NULL);
    superbloco[isuperbloco].last_mod = time(NULL);
    if (ceil(superbloco[isuperbloco].tamanho/TAM_BLOCO) == 0)
        superbloco[isuperbloco].quant_blocos = 1;
    else
      superbloco[isuperbloco].quant_blocos = ceil(superbloco[isuperbloco].tamanho/TAM_BLOCO);

    //aumetar marcacao de espaco de gravacao do arquivo
    if((gravacao_bloco_conteudo + superbloco[isuperbloco].quant_blocos) < memoria_disponivel){
        gravacao_bloco_conteudo = gravacao_bloco_conteudo + superbloco[isuperbloco].quant_blocos;
    }else{
        //significa que todos os blocos estao ocupados e nao tem espaço para escrever mais
        printf("Todos os blocos de conteudo foram usados, tamnho maximo atingido, conteudo do arquivo nao foi gravado");
        return;
    }

   if (conteudo != NULL){
        memcpy(disco + DISCO_OFFSET(bloco), conteudo,tamanho);

   }else
        memset(disco + DISCO_OFFSET(bloco), 0, tamanho);

    persistecia_write();
}


/* Para persistir o FS em um disco representado por um arquivo, talvez
   seja necessário "formatar" o arquivo pegando o seu tamanho e
   inicializando todas as posições (ou apenas o(s) superbloco(s))
   com os valores apropriados */
void init_brisafs() {

    persistecia_read();
    superbloco = (inode*) disco; //posição 0

    //Garante que não ocorre sobreescrita de dados antigos da persistencia.
    gravacao_bloco_conteudo = quant_blocos_superinode;
    for (int i = 0; i < MAX_FILES; i++) {
        gravacao_bloco_conteudo = gravacao_bloco_conteudo + superbloco[i].quant_blocos;
        if (superbloco[i].bloco == 0) { //Livre!
            break;
        }
    }

    //Cria um arquivo na mão de boas vindas
    char *nome = "UFABC SO 2019.txt";
    //Cuidado! pois se tiver acentos em UTF8 uma letra pode ser mais que um byte
    char *conteudo = "Adoro as aulas de SO da UFABC!\n"; 

    //O quant_blocos_superinode está sendo usado pelo superbloco. O primeiro livre é o +1
    preenche_bloco(0, nome, DIREITOS_PADRAO, strlen(conteudo), quant_blocos_superinode + 1, (byte*)conteudo);
}

/* Devolve 1 caso representem o mesmo nome e 0 cc */
int compara_nome (const char *a, const char *b) {
    char *ma = (char*)a;
    char *mb = (char*)b;
    //Joga fora barras iniciais
    while (ma[0] != '\0' && ma[0] == '/')
        ma++;
    while (mb[0] != '\0' && mb[0] == '/')
        mb++;
    //Cuidado! Pode ser necessário jogar fora também barras repetidas internas
    //quando tiver diretórios
    return strcmp(ma, mb) == 0;
}

/* A função getattr_brisafs devolve os metadados de um arquivo cujo
   caminho é dado por path. Devolve 0 em caso de sucesso ou um código
   de erro. Os atributos são devolvidos pelo parâmetro stbuf */
static int getattr_brisafs(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    //Diretório raiz
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    //Busca arquivo na lista de inodes
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco != 0 //Bloco sendo usado
            && compara_nome(superbloco[i].nome, path)) { //Nome bate

            stbuf->st_mode = S_IFREG | superbloco[i].direitos;
            stbuf->st_nlink = 1;
            stbuf->st_size = superbloco[i].tamanho;
            stbuf->st_atime = time(NULL);
            stbuf->st_mtime = superbloco[i].last_access;
            stbuf->st_ctime = superbloco[i].last_mod;
            stbuf->st_uid = superbloco[i].usuario;
            stbuf->st_gid = superbloco[i].grupo;
            
            return 0; //OK, arquivo encontrado
        }
    }

    //Erro arquivo não encontrado
    return -ENOENT;
}

// Remove arquivos do Sistema Operacional
static int unlink_brisafs(const char *path){
 	
    //quantidade de blocos que ficaram livres, ou seja, que devem ser movidos
    long int qnt_bloc_mov = 0;
    //local onde do superbloco onde foi zerado.
    int i;
    //Deleta o inode do bloco deletado, apagando as informações que permitem
    //rastrealo, porem não apaga o conteudo
    for (i=0; i <= MAX_FILES; i++) {
        if (superbloco[i].bloco != 0 //Bloco sendo usado
            && compara_nome(superbloco[i].nome, path)) { //Nome bate

             qnt_bloc_mov = superbloco[i].quant_blocos;
             for (int x = 0; x < 250; x++) {
                superbloco[i].nome[x]=0;
            }
            superbloco[i].direitos = 0;
            superbloco[i].tamanho = 0;
            superbloco[i].usuario = 0;
            superbloco[i].grupo = 0;
            superbloco[i].last_access = 0;
            superbloco[i].last_mod = 0;
            superbloco[i].bloco = 0;
            superbloco[i].quant_blocos = 0;
            break;
        //return 0; //OK, arquivo encontrado
        }
        if(i == MAX_FILES){
            printf("Impossivel deletar, pois arquivo não existe");
            return 0;
        }
    } 

    //seta os ponteiros de todos os outros inodes, para que o espaço do arquivo apagado seja usado
    //no superbloco isso nao seria necessario, porem decidimos fazer para manter a linearidade
    //porem nos blocos de conteudo é necessario fazer
    for (;i < MAX_FILES; i++) {
        if (superbloco[i].bloco != 0){
            superbloco[i].direitos = superbloco[i+1].direitos;
            superbloco[i].tamanho = superbloco[i+1].tamanho;
            superbloco[i].usuario = superbloco[i+1].usuario;
            superbloco[i].grupo = superbloco[i+1].grupo;
            superbloco[i].last_access = superbloco[i+1].last_access;
            superbloco[i].last_mod = superbloco[i+1].last_mod;
            superbloco[i].bloco = superbloco[i+1].bloco - qnt_bloc_mov;
            superbloco[i].quant_blocos = superbloco[i+1].quant_blocos;
            //move o conteudo na memoria.
            memcpy(disco + DISCO_OFFSET(superbloco[i].bloco), disco + DISCO_OFFSET(superbloco[i+1].bloco),superbloco[i].tamanho);
        }else
            break;
    }
    //ao final dos for tanto os meus inodes quanto meus conteudo irao estar linear.
    //volto meu ponteiro de escrita para o lugar vazio que foi aberta no final do vetor
    //vale lembrar que nao estamos pensando em paralelismo, caso um arquivo seja escrito e apagado ao mesmo tempo
    //teremos problemas por conta da possibilidade dos ponteiros se perderem e acabarem apagando o conteudo que acabou de ser inserido.
    gravacao_bloco_conteudo = gravacao_bloco_conteudo - qnt_bloc_mov;
    persistecia_write();
    return 0;
}


/* Devolve ao FUSE a estrutura completa do diretório indicado pelo
   parâmetro path. Devolve 0 em caso de sucesso ou um código de
   erro. Atenção ao uso abaixo dos demais parâmetros. */
static int readdir_brisafs(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco != 0) { //Bloco ocupado!
            filler(buf, superbloco[i].nome, NULL, 0);
        }
    }

    return 0;
}

/* Abre um arquivo. Caso deseje controlar os arquvos abertos é preciso
   implementar esta função */
static int open_brisafs(const char *path, struct fuse_file_info *fi) {
    return 0;
}

/* Função chamada quando o FUSE deseja ler dados de um arquivo
   indicado pelo parâmetro path. Se você implementou a função
   open_brisafs, o uso do parâmetro fi é necessário. A função lê size
   bytes, a partir do offset do arquivo path no buffer buf. */
static int read_brisafs(const char *path, char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi) {

    //Procura o arquivo
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco == 0) //bloco vazio
            continue;
        if (compara_nome(path, superbloco[i].nome)) {//achou!
            size_t len = superbloco[i].tamanho;// - read_pass;
            if(offset >= len){
                return 0;
            }
            if(offset + size > len){
                memcpy(buf, disco + DISCO_OFFSET(superbloco[i].bloco) + offset, len - offset);            
                return len - offset;
            }
            memcpy(buf, disco + DISCO_OFFSET(superbloco[i].bloco) + offset, size);
            
            return size;
        }
    }
    //Arquivo não encontrado
    return -ENOENT;
}

/* Função chamada quando o FUSE deseja escrever dados em um arquivo
   indicado pelo parâmetro path. Se você implementou a função
   open_brisafs, o uso do parâmetro fi é necessário. A função escreve
   size bytes, a partir do offset do arquivo path no buffer buf. */
static int write_brisafs(const char *path, const char *buf, size_t size,
                         off_t offset, struct fuse_file_info *fi) {

    /*Tem que incrementar uma parte que move os inode e os conteudos
    conforme o espaco necessario para o armazenamento muda*/
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco == 0) { //bloco vazio
            continue;
        }
        if (compara_nome(path, superbloco[i].nome)) {//achou!

            //cuida da primeira entrada do codigo para o i aqui, a posicao gravacao_bloco_conteudo, está sempre utilizada,
            //por isso quando pretendo escrever algo tenho que dar +1
            if(superbloco[i].tamanho == 0)
                gravacao_bloco_conteudo = gravacao_bloco_conteudo +1;
                
            superbloco[i].tamanho = superbloco[i].tamanho + size;
            //utilizo como referencia para saber se estou ou nao usando mais blocos que a ultima interação
            int temp_bloco = superbloco[i].quant_blocos;

            //Calcula se vai precisar de mais blocos
            printf("\nO tamanho do arquivo é: %lu\n",size);
            if (ceil(superbloco[i].tamanho/TAM_BLOCO) == 0)
                superbloco[i].quant_blocos = 1;
            else
                superbloco[i].quant_blocos = ceil(superbloco[i].tamanho/TAM_BLOCO);

            //Atribui o resultado da conta de cima
            if((gravacao_bloco_conteudo + superbloco[i].quant_blocos - temp_bloco) < memoria_disponivel){
                //Faz o acrescimo do bloco quando as quantidade calculadas forem diferentes
                //como nao tem edicao de arquivos este valor pode apenas subir.
                gravacao_bloco_conteudo = gravacao_bloco_conteudo + superbloco[i].quant_blocos - temp_bloco;
            }else{
                //significa que todos os blocos estao ocupados e nao tem espaço para escrever.
                printf("Todos os blocos de conteudo foram usados, tamnho maximo atingido, conteudo do arquivo nao foi gravado");
                return -EIO;
            }
                //utilizado para encontrar o motivo do meu ponteiro estar reiniciando a cada 15 escritas.
                //o motivo foi que estava marcado como uint16_t o que nao continha espaço suficiente, aumentei para long.
                printf("O local do vetor onde está a escrita é: %u",gravacao_bloco_conteudo);
                printf("A quantidade de blocos ocupada para o arquivo: %s é: %u\n",superbloco[i].nome,superbloco[i].quant_blocos);

                memcpy(disco + DISCO_OFFSET(superbloco[i].bloco) + offset, buf, size);

        return size;
        }
    }
    //Se chegou aqui não achou. Entao cria
    //Acha o primeiro bloco vazio
    //mudando a inicial do i, para evitar escrever nos blocos reservados
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco == 0) {//ninguem usando
            //gravacao_bloco_conteudo = gravacao_bloco_conteudo +1;
            preenche_bloco (i, path, DIREITOS_PADRAO, size, gravacao_bloco_conteudo + 1, buf);
            return size;
        }
    }

    return -EIO;
}

/* Altera o tamanho do arquivo apontado por path para tamanho size
   bytes, esta função nao foi alterada, com exceção da parte comentada dela*/
static int truncate_brisafs(const char *path, off_t size) {
    if (size > TAM_BLOCO)
        return EFBIG;

    //procura o arquivo
    int findex = -1;
    for(int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco != 0
            && compara_nome(path, superbloco[i].nome)) {
            findex = i;
            break;
        }
    }
    if (findex != -1) {// arquivo existente
        //superbloco[findex].tamanho = size + superbloco[findex].tamanho;
        return 0;
    } else {// Arquivo novo
        //Acha o primeiro bloco vazio
        for (int i = 0 + 1; i < MAX_FILES; i++) {
            if (superbloco[i].bloco == 0) {//ninguem usando
                preenche_bloco (i, path, DIREITOS_PADRAO, size, gravacao_bloco_conteudo + 1, NULL);
                break;
            }
        }
    }
    
    return 0;
}

/* Cria um arquivo comum ou arquivo especial (links, pipes, ...) no caminho
   path com o modo mode*/
static int mknod_brisafs(const char *path, mode_t mode, dev_t rdev) {
    if (S_ISREG(mode)) { //So aceito criar arquivos normais
        //Cuidado! Não seta os direitos corretamente! Veja "man 2
        //mknod" para instruções de como pegar os direitos e demais
        //informações sobre os arquivos
        //Acha o primeiro bloco vazio
        for (int i = 0; i < MAX_FILES; i++) {
            if (superbloco[i].bloco == 0) {//ninguem usando
                preenche_bloco (i, path, DIREITOS_PADRAO, 0, gravacao_bloco_conteudo + 1, NULL);
                return 0;
            }
        }
        return ENOSPC;
    }
    return EINVAL;
}

/* Sincroniza escritas pendentes (ainda em um buffer) em disco. Só
   retorna quando todas as escritas pendentes tiverem sido
   persistidas */
static int fsync_brisafs(const char *path, int isdatasync,
                         struct fuse_file_info *fi) {
    //foram montados dois metodos iniciais, sendo este metodo não necessario, porem para garantir
    //estou limpando o fflush neles tbm.
    fflush(stdin);
    fflush(stdout);
    //Como tudo é em memória, não é preciso fazer nada.
    // Cuidado! Você vai precisar jogar tudo que está só em memóri no disco
    return 0;
}

/* Ajusta a data de acesso e modificação do arquivo com resolução de nanosegundos */
static int utimens_brisafs(const char *path, const struct timespec ts[2]) {
    //Salva os arquivos de memoria no disco
    persistecia_write();
    //Busca arquivo na lista de inodes
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco != 0 //Bloco sendo usado
            && compara_nome(superbloco[i].nome, path)) { //Nome bate
            //timespec.
            superbloco[i].last_access = ts[0].tv_sec;
            superbloco[i].last_mod = ts[1].tv_sec;
        return 0; //OK, arquivo encontrado
        }
    } 
    // Cuidado! O sistema BrisaFS não aceita horários. O seu deverá aceitar!
    return 0;

/* Cria e abre o arquivo apontado por path. Se o arquivo não existir
   cria e depois abre*/
}
static int create_brisafs(const char *path, mode_t mode,
                          struct fuse_file_info *fi) {
    //Cuidado! Está ignorando todos os parâmetros. O seu deverá
    //cuidar disso Veja "man 2 mknod" para instruções de como pegar os
    //direitos e demais informações sobre os arquivos Acha o primeiro
    //bloco vazio
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco == 0) {//ninguem usando
            preenche_bloco (i, path, DIREITOS_PADRAO, 0, gravacao_bloco_conteudo + 1, NULL);
            return 0;
        }
    }
    return ENOSPC;
}

/* Função que implementa o comando CHOWN, responsavel por alterar os campos
'usuario' e 'grupo' do arquivo.*/
static int chown_brisafs(const char *path, uid_t usuario, gid_t grupo) {

    //Busca arquivo na lista de inodes
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco != 0 //Bloco sendo usado
            && compara_nome(superbloco[i].nome, path)) { //Nome bate
            superbloco[i].usuario = usuario;
            superbloco[i].grupo = grupo;
        return 0; //OK, arquivo encontrado
        }
    }     
    return 0;
}

/* Função que implementa o comando CHMOD, responsavel por alterar o campo
'direitos' do arquivo.*/
static int chmod_brisafs(const char *path, mode_t modo) {

    //Busca arquivo na lista de inodes
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco != 0 //Bloco sendo usado
            && compara_nome(superbloco[i].nome, path)) { //Nome bate
            superbloco[i].direitos = modo;
        return 0; //OK, arquivo encontrado
        }
    } 
    return 0;
}


/* Esta estrutura contém os ponteiros para as operações implementadas
   no sdasFS */
static struct fuse_operations fuse_brisafs = {
                                              .chmod = chmod_brisafs,
                                              .chown = chown_brisafs,
                                              .create = create_brisafs,
                                              .fsync = fsync_brisafs,
                                              .unlink = unlink_brisafs,
                                              .getattr = getattr_brisafs,
                                              .mknod = mknod_brisafs,
                                              .open = open_brisafs,
                                              .read = read_brisafs,
                                              .readdir = readdir_brisafs,
                                              .truncate	= truncate_brisafs,
                                              .utimens = utimens_brisafs,
.write = write_brisafs
};
//Retirado tanto a ideia quanto o pedaço para conseguir a memoria ram do:
//https://stackoverflow.com/questions/349889/how-do-you-determine-the-amount-of-linux-system-ram-in-c
//Foi modificado para utilização no projeot, mantive os comentarios originais, e a estrutura original
long GetRamInKB(void)
{
    char line[256];
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if(meminfo == NULL)
        printf("Ocorreu um erro, iniciando com memoria padra.");
    while(fgets(line, sizeof(line), meminfo))
    {
        long ram;
        //Modificado para retornar a memoria total do pc
        //deixei fixado a memoria total para ter um valor fixo de maximo, dependendo do pc
        if(sscanf(line, "MemTotal: %lu kB", &ram) == 1)
        {
            fclose(meminfo);
            //estou pegando apenas 5% da ram total.
            //porem caso a quantidade de ram do computador passe este valor: 350000.
            //que equivale a aproximadamente 1,335GB, mais o 1% dos inodes.
            //ajuste ele comom total, pois ele cumpre os 2 requisitos da nota:
            // mais de 1024 arquivos, e o tamanho maximo minimo de 1G
            if((ram*0.1)>350000)
                return 350000;
            else
                return ram*0.1;
        }
    }

    // If we got here, then we couldn't find the proper line in the meminfo file:
    // do something appropriate like return an error code, throw an exception, etc.
    fclose(meminfo);
    //Vai tentar reservar esta quantia (que eu ussei para teste em casa), caso nao consiga encontrar
    return 350000;
}


int main(int argc, char *argv[]) {

    //funcao para descobrir memoria ram utilizavel para o projeto
    memoria_disponivel = GetRamInKB();
    //memoria_disponivel = 350000;    

    //estou utilizando para controle dos status do projeto.
    printf("Iniciando o BrisaFS...\n");
    printf("\t Tamanho máximo de arquivo = n bloco = %lu byte\n", TAM_BLOCO * (MAX_BLOCOS - quant_blocos_superinode));
    printf("\t Tamanho do bloco: %u byte\n", TAM_BLOCO);
    printf("\t Tamanho do inode: %lu byte\n", sizeof(inode));
    printf("\t Número máximo de arquivos: %u\n", MAX_FILES);
    printf("\t Quantidade de blocos para conter o superbloco de %u arquivos: %lu blocos\n",MAX_FILES, quant_blocos_superinode);
    printf("\t Número máximo de blocos: %lu\n", MAX_BLOCOS);
    printf("\t Memoria RAM que será usada nós blocos: %u KB + 0.1%% para os inode\n",memoria_disponivel);

    //Verifica a quantidade de memória RAM disponivel no sistema operacional
    init_brisafs();

    return fuse_main(argc, argv, &fuse_brisafs, NULL);
}
