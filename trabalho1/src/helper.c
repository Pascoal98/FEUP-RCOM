
#include "link_layer.h"

// ^ = XOR

//bbc1
unsigned char createBCC_header(unsigned char address, unsigned char control) {
    return address ^ control;
}

//bcc2
unsigned char createBCC_data(unsigned char* frame, int length) {

  unsigned char bcc = frame[0];

  for(int i = 1; i < length; i++){
    bcc = bcc ^ frame[i];
  }

  return bcc;
}


int createInformationFrame(unsigned char* frame, unsigned char control, unsigned char* info, int infolength) {

  frame[0] = FLAG;

  frame[1] = A_SEND; 

  frame[2] = control;

  frame[3] = createBCC_header(frame[1], frame[2]);

  for(int i = 0; i < infolength; i++) {
    frame[i + 4] = info[i];
  }

  unsigned bcc2 = createBCC_data(info, infolength);

  frame[infolength + 4] = bcc2;

  frame[infolength + 5] = FLAG;

  return 0;
}


int byteStuffing(unsigned char* frame, int length) {


  unsigned char aux[length + 6];

  for(int i = 0; i < length + 6 ; i++){
    aux[i] = frame[i];
  }


  
  int finalLength = ;  //onde a informaçao a enviar começa

 
  for(int i = DATA_START; i < (length + 6); i++){

    if(aux[i] == FLAG && i != (length + 5)) {
      frame[finalLength] = ESC_BYTE;
      frame[finalLength+1] = BYTE_STUFFING_FLAG;
      finalLength = finalLength + 2;
    }
    else if(aux[i] == ESC_BYTE && i != (length + 5)) {
      frame[finalLength] = ESC_BYTE;
      frame[finalLength+1] = BYTE_STUFFING_ESCAPE;
      finalLength = finalLength + 2;
    }
    else{
      frame[finalLength] = aux[i];
      finalLength++;
    }
  }

  return finalLength;
}


int byteDestuffing(unsigned char* frame, int length) {

  
  unsigned char aux[length + 5];

  //copiar da trama para o auxiliar
  for(int i = 0; i < (length + 5) ; i++) {
    aux[i] = frame[i];
  }

  int finalLength = DATA_START;

  // iterates through the aux buffer, and fills the frame buffer with destuffed content
  for(int i = DATA_START; i < (length + 5); i++) {

    if(aux[i] == ESC_BYTE){
      if (aux[i+1] == BYTE_STUFFING_ESCAPE) {
        frame[finalLength] = ESC_BYTE;
      }
      else if(aux[i+1] == BYTE_STUFFING_FLAG) {
        frame[finalLength] = FLAG;
      }
      i++;
      finalLength++;
    }
    else{
      frame[finalLength] = aux[i];
      finalLength++;
    }
  }

  return finalLength;
}


