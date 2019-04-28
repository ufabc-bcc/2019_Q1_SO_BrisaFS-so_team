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
        Persistência - Incluindo a criação e "formatação" de um arquivo novo para conter o seu "disco". --FEITO
            Veja a função ftruncate para criar um arquivo com o tamanho pré-determinado
        Armazenamento e recuperação de datas (via ls por exemplo)   --FEITO
        Armazenamento e alteração direitos usando chown e chgrp
        Aumento do número máximo de arquivos para pelo menos 1024   --FEITO

    Nota 7
        Aumento do tamanho máximo do arquivo para pelo menos 64 MB --Iniciado
        Suporte à criação de diretórios
        Exclusão de arquivos

    Nota 10
        Suporte a "discos" de tamanhos arbitrários
        Arquivos com tamanho máximo de pelo menos 1GB
        Controle de arquivos abertos/fechados

    Nota 12
        Funcionalidades julgadas excepcionais além das pedidas podem gerar um bônus de até 2 pontos. Converse com o professor para saber se a sua ideia é considerada excepcional e quanto ela vale.

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
#define MAX_FILES 2048
/* Para o superbloco e o resto para os arquivos. Os arquivos nesta
   implementação também tem uma quantidade de bloco, para conseguir guardar aquivos maiores que 1 G
   Se cada arquivo puder usar mais de 1 bloco. */
#define MAX_BLOCOS (500000 + quant_blocos_superinode)
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
    uint16_t tamanho;
    time_t data1;
    time_t data2;
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
int gravacao_bloco_conteudo;

#define DISCO_OFFSET(B) (B * TAM_BLOCO)

void persistecia_write(){
    Tmp_disco = fopen("Persistencia","wb");
    fwrite(disco,1,(gravacao_bloco_conteudo+1)*TAM_BLOCO,Tmp_disco);
    fflush(stdout);
    fclose(Tmp_disco);    
}

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
    superbloco[isuperbloco].tamanho = tamanho;
    superbloco[isuperbloco].bloco = bloco;
    //Iniciar com as datas preenchidas
    //conforme usa a funcao utimens_brisafs cuidas das datas, esta funcao já foi implementada pelo grupo
    superbloco[isuperbloco].data1 = time(NULL);
    superbloco[isuperbloco].data2 = time(NULL);
    if (ceil(tamanho/TAM_BLOCO) == 0)
        superbloco[isuperbloco].quant_blocos = 1;
    else
      superbloco[isuperbloco].quant_blocos = ceil(tamanho/TAM_BLOCO);

    //aumetar marcacao de espaco de gravacao do arquivo
    if((gravacao_bloco_conteudo + superbloco[isuperbloco].quant_blocos) < 500000){
        gravacao_bloco_conteudo = gravacao_bloco_conteudo + superbloco[isuperbloco].quant_blocos;
    }else{
        //significa que todos os blocos estao ocupados e nao tem espaço para escrever mais
        printf("Todos os blocos de conteudo foram usados, tamnho maximo atingido, conteudo do arquivo nao foi gravado");
        return;
    }

   if (conteudo != NULL){
       for (int i = 0; i < superbloco[isuperbloco].quant_blocos; i++) {
            if(i != superbloco[isuperbloco].quant_blocos-1){
                printf("Passei por aqui 1");
                memcpy(disco + DISCO_OFFSET(bloco) + (i*TAM_BLOCO), conteudo + (i*TAM_BLOCO), TAM_BLOCO);
            }
            else{
                printf("Passei por aqui 2");            
                memcpy(disco + DISCO_OFFSET(bloco) + (i*TAM_BLOCO), conteudo + (i*TAM_BLOCO), tamanho - floor(tamanho/TAM_BLOCO) * TAM_BLOCO);
            }
        }
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
            if(i > 0)
                gravacao_bloco_conteudo = quant_blocos_superinode + i -1;
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
            printf("%ld\n",superbloco[i].data1);
            printf("%ld\n",superbloco[i].data2);

            stbuf->st_mtime = superbloco[i].data1;
            stbuf->st_ctime = superbloco[i].data2;

            return 0; //OK, arquivo encontrado
        }
    }

    //Erro arquivo não encontrado
    return -ENOENT;
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
            size_t len = superbloco[i].tamanho;
            /*if (offset >= len) {//tentou ler além do fim do arquivo
                return 0;
            }*/
            if (offset + size > len) {
                memcpy(buf, disco + DISCO_OFFSET(superbloco[i].bloco),len - offset);
                return len - offset;
            }
            memcpy(buf, disco + DISCO_OFFSET(superbloco[i].bloco), size);
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

    /*
    Tem que incrementar uma parte que move os inode e os conteudos
    conforme o espaco necessario para o armazenamento muda*/
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco == 0) { //bloco vazio
            continue;
        }
        if (compara_nome(path, superbloco[i].nome)) {//achou!
            // Cuidado! Não checa se a quantidade de bytes cabe no arquivo!
            memcpy(disco + DISCO_OFFSET(superbloco[i].bloco) + offset, buf, size);
            superbloco[i].tamanho = offset + size;
            return size;
        }
    }
    //Se chegou aqui não achou. Entao cria
    //Acha o primeiro bloco vazio
    //mudando a inicial do i, para evitar escrever nos blocos reservados
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco == 0) {//ninguem usando
            preenche_bloco (i, path, DIREITOS_PADRAO, size, gravacao_bloco_conteudo + 1, buf);
            return size;
        }
    }

    return -EIO;
}


/* Altera o tamanho do arquivo apontado por path para tamanho size
   bytes */
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
        superbloco[findex].tamanho = size;
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

        //Busca arquivo na lista de inodes
    for (int i = 0; i < MAX_FILES; i++) {
        if (superbloco[i].bloco != 0 //Bloco sendo usado
            && compara_nome(superbloco[i].nome, path)) { //Nome bate
            //timespec.
            superbloco[i].data1 = ts[0].tv_sec;
            superbloco[i].data2 = ts[1].tv_sec;
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


/* Esta estrutura contém os ponteiros para as operações implementadas
   no sdasFS */
static struct fuse_operations fuse_brisafs = {
                                              .create = create_brisafs,
                                              .fsync = fsync_brisafs,
                                              .getattr = getattr_brisafs,
                                              .mknod = mknod_brisafs,
                                              .open = open_brisafs,
                                              .read = read_brisafs,
                                              .readdir = readdir_brisafs,
                                              .truncate	= truncate_brisafs,
                                              .utimens = utimens_brisafs,
                                              .write = write_brisafs
};

int main(int argc, char *argv[]) {

    printf("Iniciando o BrisaFS...\n");
    printf("\t Tamanho máximo de arquivo = n bloco = %lu bytes\n", TAM_BLOCO * (MAX_BLOCOS - quant_blocos_superinode));
    printf("\t Tamanho do bloco: %u\n", TAM_BLOCO);
    printf("\t Tamanho do inode: %lu\n", sizeof(inode));
    printf("\t Número máximo de arquivos: %u\n", MAX_FILES);
    printf("\t Quantidade de blocos para conter o superbloco de 2048 arquivos: %lu\n", quant_blocos_superinode);
    printf("\t Número máximo de blocos: %lu\n", MAX_BLOCOS);
    //printf("\t Comparando tamanhos %ld\n",strlen("Adoro as aulas de SO da UFABC!\n"));
    init_brisafs();

    return fuse_main(argc, argv, &fuse_brisafs, NULL);
}
