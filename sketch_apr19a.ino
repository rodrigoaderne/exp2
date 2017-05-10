/* Protocolo de aplicacao - Implementacao usando rotina de interrupcao e
 *  controle.
 *
 *  Uso:
 *  - Computador envia (pelo terminal) uma mensagem:
 *  PING\n\n
 *  - MCU retorna (no terminal):
 *  PONG\n
 *
 *  Tiago F. Tavares
 *  GPL3.0 - 2017
 */

/* stdio.h contem rotinas para processamento de expressoes regulares */
#include <stdio.h>
#include <Wire.h>

int posicao;
//A/D
int ldrPin = 0; //LDR no pino analígico 8
int ldrValor = 0; //Valor lido do LDR
int C1 = 8, C2 = 9, C3 = 10, L1 = 11, L2 = 12, L4 = 13; //variaveis do teclado matricial
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

void eeprom_i2c_write(byte address, byte from_addr, byte data) {
  Wire.beginTransmission(address);
  Wire.write(from_addr);
  Wire.write(data);
  Wire.endTransmission();
}

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

void RECORD() {
    ldrValor = analogRead(ldrPin)*5/1024 ; //LEITURA E CONVERSAO DO VALOR DO LDR.
    eeprom_i2c_write(0x50, posicao, ldrValor);  // grava valor na EEPROM.
    posicao++; //incrementa posição de memória.                                                   
    delay(100);
    eeprom_i2c_write(0x50, 0, posicao);  // grava novo indice da memoia na posição 0 da memoria.
    buffer_clean();   
  }

/* Funcoes internas ao void main() */

void setup() {

  /* configuração de pinos para teclado matricial */
  pinMode(L1, OUTPUT);
  pinMode(L2, OUTPUT);
  pinMode(L4, OUTPUT);
  pinMode(C1, INPUT_PULLUP);
  pinMode(C2, INPUT_PULLUP);
  pinMode(C3, INPUT_PULLUP);
  
  pinMode(LED, OUTPUT);
  
  /* Inicializacao */
  buffer_clean();
  flag_check_command = 0;
  Serial.begin(9600);
  Wire.begin();
  
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
    if (str_cmp(Buffer.data, "PING", 4) ) {
      sprintf(out_buffer, "PONG\n");
      flag_write = 1;
    }

    if (str_cmp(Buffer.data, "ID", 2) ) {
      sprintf(out_buffer, "XOTE");
      flag_write = 1;
    }

    if (str_cmp(Buffer.data, "SUM", 3) ) {
      sscanf(Buffer.data, "%*s %d %d", &x, &y);
      sprintf(out_buffer, "SUM = %d\n", x+y);
      flag_write = 1;
    }

    if (str_cmp(Buffer.data, "MEASURE", 7) ) {
      ldrValor = analogRead(ldrPin); //LEITURA E CONVERSAO DO VALOR DO LDR.
      sprintf(out_buffer, "LIGHT = %d V \n", ldrValor*5/1024);
      flag_write = 1;
    }
    
    /*Numero de elementos na memoria*/
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
    
    /*Realiza uma medição e grava o valor na memóoria.*/
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

  /*TRATAMENTO DO TECLADO MATRICIAL*/

  
  
// /* verifica se os botões 1, 2 ou 3 da linha 1 foram precionados. */
//  digitalWrite(L1, LOW);
//  digitalWrite(L2, HIGH);
//  
//  /* testa se tecla 1 foi pressionada, caso sim, Pisca um LED, indicando que o sistema esta responsivo. */
//  if (digitalRead(C1) == LOW) { 
//   digitalWrite(LED, HIGH);
//   delay(500);
//   digitalWrite(LED, LOW);
//   delay(500);
//   while(digitalRead(C1) == LOW);
//  }
//
//  /* testa se a tecla 2 foi pressionada, caso sim, Realiza uma medição e grava o valor na memória. */
//  if (digitalRead(C2) == LOW) { 
//    RECORD();
//    while(digitalRead(C2) == LOW); 
//  }
//
//  /*  testa se a tecla 3 foi pressionada, caso sim, Ativa modo de medição automática. */
//  if (digitalRead(C3) == LOW) {
//   /* desativa linha 1 da leitura e ativa linha 2 para leitura do botão 4 */
//  digitalWrite(L1, HIGH);
//  digitalWrite(L2, LOW);
//   /* enquanto tecla 4 não é pressionada, realiza uma leitura e gravação na EEPROM a cada 1s. Quando 4 for pressionado, desativa medição automatica */
//    while (digitalRead(C1) == HIGH){ 
//         RECORD();
//         delay(1000);
//    }
//  }
//    

}
