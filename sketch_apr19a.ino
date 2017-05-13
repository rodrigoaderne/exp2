/*DATALOGGER RODRIGO E GUSTAVO
 * Datalogger para leitura e gravação em memória EEMRPOM de sensor LDR com controle via UART e teclado matricial. Esta implimentado para a UART as funções:
 * - PING: sistema deve responder PONG
 * - ID: sistema responde EA076
 * - MEASURE: Retorna o valor de uma medição, mas não grava na memória.
 * - MEMSTATUS: Número de elementos na memória 
 * - RESET: Apaga toda a memória 
 * - RECORD: Realiza uma medição e grava o valor na memória.
 * - GET N: Retorna o N-ésimo elemento na memória
 * 
 * Para teclado matricial as seguintes funções foram implementadas:
 * - #1*: Pisca um LED, indicando que o sistema está responsivo. 
 * - #2*: Realiza uma medição e grava o valor na memória. 
 * - #3*: Ativa modo de medição automática
 * - #4*: Encerra modo de medição automática
 */

#include <stdio.h>
#include <Wire.h>
#include <TimerOne.h>

int posicao;
int ldrPin = 0; //LDR no pino analígico 8
int ldrValor = 0; //Valor lido do LDR
int C1 = 8, C2 = 9, C3 = 10, L1 = 11, L2 = 12, L3 = 13, L4 = 4; //entradas (C1, C2 e C3) e saidas  do teclado matricial
int int_gravar = 0, grava_automatico = 0, tecla_hashtag = 0, segundatecla = 0, tecla_estrela = 0; //variaveis para controle do teclado
int LED = 7; 

/* Rotina auxiliar para comparacao de strings */
int str_cmp(char *s1, char *s2, int len) {
  /* Compare two strings up to length len. Return 1 if they are
   *  equal, and 0 otherwise.
   */
  int i;
  for (i=0; i<len; i++) {
    if (s1[i] != s2[i]) return 0;
    if (s1[i] == '\0') return 1;
  }
  return 1;
}

/* Processo de bufferizacao. Caracteres recebidos sao armazenados em um buffer. Quando um caractere
 *  de fim de linha ('\n') e recebido, todos os caracteres do buffer sao processados simultaneamente.
 */

/* Buffer de dados recebidos */
#define MAX_BUFFER_SIZE 15
typedef struct {
  char data[MAX_BUFFER_SIZE];
  unsigned int tam_buffer; 
}serial_buffer;

/* Teremos somente um buffer em nosso programa, O modificador volatile
 *  informa ao compilador que o conteudo de Buffer pode ser modificado a qualquer momento. Isso
 *  restringe algumas otimizacoes que o compilador possa fazer, evitando inconsistencias em
 *  algumas situacoes (por exemplo, evitando que ele possa ser modificado em uma rotina de interrupcao
 *  enquanto esta sendo lido no programa principal).
 */
 serial_buffer Buffer;

/* Todas as funcoes a seguir assumem que existe somente um buffer no programa e que ele foi
 *  declarado como Buffer. Esse padrao de design - assumir que so existe uma instancia de uma
 *  determinada estrutura - se chama Singleton (ou: uma adaptacao dele para a programacao
 *  nao-orientada-a-objetos). Ele evita que tenhamos que passar o endereco do
 *  buffer como parametro em todas as operacoes (isso pode economizar algumas instrucoes PUSH/POP
 *  nas chamadas de funcao, mas esse nao eh o nosso motivo principal para utiliza-lo), alem de
 *  garantir um ponto de acesso global a todas as informacoes contidas nele.
 */

/* Limpa buffer */
void buffer_clean() {
  Buffer.tam_buffer = 0;
}

/* Adiciona caractere ao buffer */
int buffer_add(char c_in) {
  if (Buffer.tam_buffer < MAX_BUFFER_SIZE) {
    Buffer.data[Buffer.tam_buffer++] = c_in;
    return 1;
  }
  return 0;
}

/* Flags globais para controle de processos da interrupcao */
 int flag_check_command = 0;

/* Rotinas de interrupcao */

/* Ao receber evento da UART */
void serialEvent() {
  char c;
  while (Serial.available()) {
    c = Serial.read();
    if (c=='\n') {
      buffer_add('\0'); /* Se recebeu um fim de linha, coloca um terminador de string no buffer */
      flag_check_command = 1;
    } else {
     buffer_add(c);
    }
  }
}

/* rotina para escrever na EEPROM*/
void eeprom_i2c_write(byte address, byte from_addr, byte data) {
  Wire.beginTransmission(address);
  Wire.write(from_addr);
  Wire.write(data);
  Wire.endTransmission();
}

/*rotina para leitura da EEPROM*/
byte eeprom_i2c_read(int address, int from_addr) {
  Wire.beginTransmission(address);
  Wire.write(from_addr);
  Wire.endTransmission();

  Wire.requestFrom(address, 1);
  if(Wire.available())
    return Wire.read();
  else
    return 0xFF;
}

/*rotina para leitura e conversao do sensor LDR e gravação do valor na EEPROM do respectivo valor*/
void RECORD() {  
  ldrValor = analogRead(ldrPin)*5/1024 ; //LEITURA E CONVERSAO DO VALOR DO LDR.
  eeprom_i2c_write(0x50, posicao, ldrValor);  // grava valor na EEPROM.
  posicao++; //incrementa posição de memória.                                                   
  delay(100); //delay para aguardar rotina de escrita da EEPROM
  eeprom_i2c_write(0x50, 0, posicao);  // grava novo indice da memoia na posição 0 da memoria.
  buffer_clean();   
}

/*interrupção a cada 1s para gravação automatica de valores do sensor na EEPROM*/
void ISR_timer(){
 if(grava_automatico) //se a gravação automatica esta ativa aciona flag para gravação no loop principal.
    int_gravar = 1; 
}

/* Funcoes internas ao void main() */
void setup() {
  /* configuração de pinos para teclado matricial */
  pinMode(L1, OUTPUT); //linhas do teclado matricial
  pinMode(L2, OUTPUT);
  pinMode(L3, OUTPUT);
  pinMode(L4, OUTPUT);
  pinMode(C1, INPUT_PULLUP); //colunas do teclado matricial com resistor de pullup ativado
  pinMode(C2, INPUT_PULLUP);
  pinMode(C3, INPUT_PULLUP);
  
  pinMode(LED, OUTPUT); //led 
    
  /* Inicializacao */
  buffer_clean();
  flag_check_command = 0;
  Serial.begin(9600);
  Wire.begin();
  
  // Inicializar a rotina de interrupcao periodica para gravação automatica
  Timer1.initialize(1000000); // Interrupcao a cada 1s
  Timer1.attachInterrupt(ISR_timer); // Associa a interrupcao periodica a funcao ISR_timer
  
  posicao = eeprom_i2c_read(0x50, 0);  // Lê em qual posição esta o ultimo elemento gravado na EEPROM.


}

void loop() {
  int x, y;
  char out_buffer[10];
  int flag_write = 0;
  
  /* A flag_check_command permite separar a recepcao de caracteres
   *  (vinculada a interrupca) da interpretacao de caracteres. Dessa forma,
   *  mantemos a rotina de interrupcao mais enxuta, enquanto o processo de
   *  interpretacao de comandos - mais lento - nao impede a recepcao de
   *  outros caracteres. Como o processo nao 'prende' a maquina, ele e chamado
   *  de nao-preemptivo.
   */
  if (flag_check_command == 1) {
    /*caso escreva PING na UART, o progama responde PONG, sinalizando o funcionamento*/
    if (str_cmp(Buffer.data, "PING", 4) ) {
      sprintf(out_buffer, "PONG\n");
      flag_write = 1;
    }

     /*caso escreva ID na UART, o progama responde EA076, sinalizando o funcionamento*/
    if (str_cmp(Buffer.data, "ID", 2) ) {
      sprintf(out_buffer, "EA076");
      flag_write = 1;
    }

    /*rotina que realiza soma de dois valores na UART, deve-se usar a sintax: SUM x y*/
    if (str_cmp(Buffer.data, "SUM", 3) ) {
      sscanf(Buffer.data, "%*s %d %d", &x, &y);
      sprintf(out_buffer, "SUM = %d\n", x+y);
      flag_write = 1;
    }

    /*caso escrea MEASURE na UART, responde o valor atual da conversao do LDR*/
    if (str_cmp(Buffer.data, "MEASURE", 7) ) {
      ldrValor = analogRead(ldrPin); //LEITURA E CONVERSAO DO VALOR DO LDR.
      sprintf(out_buffer, "LIGHT = %d V \n", ldrValor*5/1024);
      flag_write = 1;
    }
    
    /*caso escreva MEASURE na UART será informado quantos elementos estão gravados na EEPROM*/
    if (str_cmp(Buffer.data, "MEMSTATUS", 9) ) {
      sprintf(out_buffer, "%d  \n", posicao - 1); //imprime no terminal a quantidade de elementos gravados na memoria.
      flag_write = 1;
    }
   
   /*Apaga toda a memoria*/
    if (str_cmp(Buffer.data, "RESET", 5) ) {
      posicao = 1; // volta indice da memoria para posição inicial
      eeprom_i2c_write(0x50, 0, 1);  // grava valor na EEPROM do novo indice.
      buffer_clean();
    }
    
    /*Realiza uma medição do LDR e grava o valor na EEPROM.*/
    if (str_cmp(Buffer.data, "RECORD", 6) ) {
      RECORD();
    }
    
    /*Retorna o N-ésimo elemento na memória*/
    if (str_cmp(Buffer.data, "GET", 3) ) {
      sscanf(Buffer.data, "%*s %d", &x); //lê qual posição de memoria deseja
      sprintf(out_buffer, "%d\n", eeprom_i2c_read(0x50, x)); //lê a posição de memória correnspondente e imprime no terminal
      flag_write = 1;
    }
 
    flag_check_command = 0;
  }

  /* Posso construir uma dessas estruturas if(flag) para cada funcionalidade
   *  do sistema. Nesta a seguir, flag_write e habilitada sempre que alguma outra
   *  funcionalidade criou uma requisicao por escrever o conteudo do buffer na
   *  saida UART.
   */
  if (flag_write == 1) {
    Serial.write(out_buffer);
    buffer_clean();
    flag_write = 0;
  }

///////////////////////////////////////////////////TRATAMENTO DO TECLADO MATRICIAL/////////////////////////////////////////////////////////////////////////////////

/*abaixo está o codigo para controle de leitura do teclado matricial, a logica é baseada na sequencia das teclas pré determinadas, podendo ser
  #1*, #2*, #3* ou  #4*, caso seja apertado a tecla # o programa ira aguadar que na sequencia seja aperta 1, 2,3 ou 4 e após isso irá aguardar
  que seja apertado a tecla *. Caso em algum momento da sequencia seja identificado uma sequencia errada, reinici-se a leitura do principio,
  exemplo: caso seja teclado #11 é reiniciado,caso #1# é reiniciado.*/
  
 /* configura as saidas  para as linhas do teclado matricial para realizar leitura da linha 4: teclas #, 0 ou *.  */
  digitalWrite(L1, HIGH);
  digitalWrite(L2, HIGH);
  digitalWrite(L3, HIGH);
  digitalWrite(L4, LOW);

  /* testa se tecla # foi pressionada e aciona flag para verifacação da segunda tecla. */
    if (!digitalRead(C1)) {
     tecla_hashtag = 1;
  }
  
  //rotina para teste da segunda tecla, somenente entra nesta rotina caso a tecla # já tenha sido pressionada
  if (tecla_hashtag){
    /* configura as saidas  para as linhas do teclado matricial para realizar leitura da linha 1: teclas 1, 2 ou 3.  */
    digitalWrite(L1, LOW );
    digitalWrite(L2, HIGH);
    digitalWrite(L4, HIGH);

     /*abaixo estão as rotinas para testes das teclas 1, 2 ou 3. caso alguma delas sejam pressionadas, grava o que foi lido
      * na variavel segundatecla e reseta flag da primeira tecla para nao voltar a rotina da segunda tecla.
      */
    
    // caso tenha pressionado a tecla 1 após pressionar a tecla #
    while (!digitalRead(C1)) {
      segundatecla = 1;
      tecla_hashtag = 0; 
      }  
       
    // caso tenha pressionado a tecla 2 após pressionar a tecla # 
     while (!digitalRead(C2)) {
      segundatecla = 2;
      tecla_hashtag = 0;
      }
      
    // caso tenha pressionado a tecla 3 após pressionar a tecla #
     while (!digitalRead(C3)) {
      segundatecla = 3;  
      tecla_hashtag = 0;
     }

    /* configura as saidas  para as linhas do teclado matricial para realizar leitura da linha 1: tecla 4  */
    digitalWrite(L1, HIGH);
    digitalWrite(L2, LOW);
    digitalWrite(L4, HIGH);
  
    // caso tenha pressionado a tecla 4 após pressionar a tecla #
    while (!digitalRead(C1)) {
      segundatecla = 4;  
      tecla_hashtag = 0;
    }
  }

  /*Caso a segunda tecla tenha sido pressionada, entra na rotina abaixo para teste da terceira tecla, que deve ser obrigatoriamente a tecla *,
  caso qualquer outra tecla seja pressionada o programa volta para o estado inicial de teste da tecla #*/
  if(segundatecla){
    /* configura as saidas  para as linhas do teclado matricial para realizar leitura da linha 1, 2 e 3: teclas 1 a 9.  */
    digitalWrite(L1, LOW);
    digitalWrite(L2, LOW);
    digitalWrite(L3, LOW);
    digitalWrite(L4, HIGH);

    //caso qualquer umas das teclas de 1 a 9 sejam pressionadas, reseta a flag, voltando a logica para o estado inical
    while (!digitalRead(C1) || !digitalRead(C2) || !digitalRead(C3))
      segundatecla = 0;
      
    digitalWrite(L1, HIGH);
    digitalWrite(L2, HIGH);
    digitalWrite(L3, HIGH);    
    digitalWrite(L4, LOW);

    //caso as das teclas # ou 0 sejam pressionadas, reseta a flag, voltando a logica para o estado inical
    while(!digitalRead(C1) || !digitalRead(C2))
      segundatecla = 0;

    //caso a tecla * seja pressionada, aciona flag para entrar na rotina de execução da tarefa selecionada
    while (!digitalRead(C3))
      tecla_estrela = 1;
  }

  /* rotina para execução da tarefa seleciona pelas teclas*/
  if(tecla_estrela){
    switch (segundatecla){

      // caso 1: #1*, pisca led indicando que o sistema esta responsivo
      case 1: {
        digitalWrite(LED, HIGH);
        delay(500);
        digitalWrite(LED, LOW);
        delay(500);        
        }
        break;

      // caso 2: #2*, realiza a leitura do sensor e grava valor na EEPROM
      case 2:{
        RECORD();        
      }
      break;

      // caso 3: #3*, inicia leiutura automatica do LDR com gravação na EEPROM
      case 3:{
        grava_automatico = 1;        
      }
      break;

      // caso 4: #4*, para leitura automatica
      case 4:{
        grava_automatico = 0;  
      }
      break;
   }
  //volta logica para estado inical
  tecla_estrela = 0;
  segundatecla = 0;
  }

 //caso esteja ativado leitura automatica, aguarda resposta da rotina de interrupção ISR_timer e realiza gravação na EEPROM
 if(grava_automatico == 1 && int_gravar == 1){
  RECORD();
  int_gravar = 0;
  } 
}
