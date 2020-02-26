

#include <Arduino.h>

// prime
uint32_t p = 2147483647;
// generator in
uint32_t g = 16807;
//the pin is not connected to anything,
//will cause the number read to fluctuate
uint16_t analogPin = 1;

void setup(){
  init();
  Serial.begin(9600);
  Serial3.begin(9600);
  return;
}

//creat a function to get a random number as
// private key
uint32_t get_random(){

  //index
  uint16_t i;
  //set initial variable to store random number
  uint32_t random_num = 0;

  //use loop to get 16 bit random number
  for (i = 0; i <= 31; i++){

    // shift the number over and add a random bit
    random_num = (random_num<<1) | (analogRead(analogPin)&1);

    //pause to allow the voltage on the pin fluctuate
    delay(50);
  }

  return random_num;
}
//after learning the powModFast example from class,
//we want multiply fast,
//use plus the steps would smaller multiply steps
uint32_t multiModFast(uint32_t a,uint32_t b, uint32_t m){
  //first set initial result = 0, since we use plus
  // 0+x =x
	uint32_t result = 0;
	uint32_t multinum = b%m;
	uint32_t newB = a;
	while (newB > 0){
    //check the least significant position is 1
    //if it is 0, no need to do plus
		if (newB &1 == 1){
			result = (result + multinum)%m;
		}
    //let multinum to be twice, it's for next time calculation
		multinum = (multinum + multinum)%m;
    //since we done the current position, shift to next position
		newB = (newB >> 1);
	}
	return result;
}

//most of following powModFast is based on zac google drive
uint32_t powModFast(uint32_t a, uint32_t b, uint32_t m) {
	// compute ap[0] = a
	// ap[1] = ap[0]*ap[0]
	// ...
	// ap[i] = ap[i-1]*ap[i-1] (all mod m) for i >= 1

	uint32_t result = 1%m;
	uint32_t sqrVal = a%m; //stores a^{2^i} values, initially 2^{2^0}
	uint32_t newB = b;

	//  result = a^{binary number represented the first i bits of b}
	//  sqrVal = a^{2^i}
	//  newB = b >> i
	while (newB > 0) {
		if (newB & 1 == 1) { // evalutates to true iff i'th bit of b is 1
			result = multiModFast(result,sqrVal,m);
		}
		sqrVal = multiModFast(sqrVal,sqrVal,m);
		newB = (newB>>1);
	}

	// upon termination: newB == 0
	// so b >> i is 0
	// so result a^{binary number represented the first i bits of b} = a^b, DONE!

	return result;
}

//the function for time out.
bool wait_on_serial3( uint8_t nbytes, long timeout ) {
  unsigned long deadline = millis() + timeout;//wraparound not a problem
  while (Serial3.available()<nbytes && (timeout<0 || millis()<deadline))
  {
    delay(1); // be nice, no busy loop
  }
  return Serial3.available()>=nbytes;
}

//global values
uint32_t ckey;
uint32_t skey;

//hand shake for client
bool Clientstate(uint32_t ckey) {
  //the states for client
	enum State {Start, WaitingForACK, DataExchange};
  //initial state
	State currentState = Start;

	while (currentState != DataExchange) {

		if (currentState == Start){
        //send C and ckey
				Serial3.write('C');
				Serial3.write((char) (ckey >> 0));
  			Serial3.write((char) (ckey >> 8));
  			Serial3.write((char) (ckey >> 16));
  			Serial3.write((char) (ckey >> 24));
        //the state change to next state
        currentState = WaitingForACK;

		}

    else if((currentState == WaitingForACK)){
      //time out
      if (wait_on_serial3 (5,1000)==0){
        //go back to Start
        currentState = Start;
        continue;
      }

      char ACK = Serial3.read();
      if (ACK == 'A') {
        //store skey
			  skey = 0;
			  skey = skey | ((uint32_t) Serial3.read()) << 0;
  		  skey = skey | ((uint32_t) Serial3.read()) << 8;
  		  skey = skey | ((uint32_t) Serial3.read()) << 16;
  		  skey = skey | ((uint32_t) Serial3.read()) << 24;

        //if read "A", go to DataExchange
      Serial3.write('A');
      currentState = DataExchange;
    }
    else {
      currentState = Start;
    }
    }
		}
	return skey;
}

//hand shake for Server
bool ServerState(uint32_t skey){
  //the states of server
	enum State {Listen, WaitingForKey1, WaitForAck1, WaitingForKey2, WaitForAck2, DataExchange};
  //initial state is Listen
  State currentState = Listen;

	while (true) {

		if (currentState == Listen && Serial3.read() == 'C'){
      currentState = WaitingForKey1;

    }
		else if (currentState == WaitingForKey1){

      // if not time out
    if (wait_on_serial3 (4,1000)==1){
      //store ckey
			ckey = 0;
			ckey = ckey | ((uint32_t) Serial3.read()) << 0;
  		ckey = ckey | ((uint32_t) Serial3.read()) << 8;
  		ckey = ckey | ((uint32_t) Serial3.read()) << 16;
  		ckey = ckey | ((uint32_t) Serial3.read()) << 24;

      //send A and skey
			Serial3.write('A');
      Serial3.write((char) (skey >> 0));
      Serial3.write((char) (skey >> 8));
      Serial3.write((char) (skey >> 16));
      Serial3.write((char) (skey >> 24));
			currentState = WaitForAck1;
      }
      //time out
      else {
        currentState = Listen;
        continue;
      }
		}


		else if (currentState == WaitForAck1){
      //time out
		 if(wait_on_serial3 (1,1000)==0){
       currentState = Listen;
       continue;
     }
      //read "C" or "A"
     char in = Serial3.read();
		  if (in == 'C'){
        currentState = WaitingForKey2;
      }
      else if (in == 'A'){
        currentState = DataExchange;
      }
		}


		else if (currentState == WaitingForKey2){
      //time out
      if(wait_on_serial3 (4,1000)==0){
        currentState = Listen;
        continue;
      }
      //if not time out, store ckey
			uint32_t ckey = 0;
			ckey = ckey | ((uint32_t) Serial3.read()) << 0;
			ckey = ckey | ((uint32_t) Serial3.read()) << 8;
			ckey = ckey | ((uint32_t) Serial3.read()) << 16;
			ckey = ckey | ((uint32_t) Serial3.read()) << 24;
			currentState = WaitForAck2;
		}


		else if (currentState == WaitForAck2){
      //time out
      if(wait_on_serial3 (1,1000)==0){
        currentState = Listen;
        continue;
      }
      //read "C" or "A"
      char in = Serial3.read();
			if(in == 'C'){
        currentState = WaitingForKey2;
      }
			else if (in == 'A'){
        currentState = DataExchange;
      }

		}

    else if (currentState==DataExchange){
      break;
    }
	}
	return ckey;
}


/*the next key function from eclass
 *Based on
 * http://www.firstpr.com.au/dsp/rand31/rand31-park-miller-carta.cc.txt
 */
uint32_t next_key(uint32_t current_key) {
  const uint32_t modulus = 0x7FFFFFFF; // 2^31-1
  const uint32_t consta = 48271;  // we use that consta<2^16
  uint32_t lo = consta*(current_key & 0xFFFF);
  uint32_t hi = consta*(current_key >> 16);
  lo += (hi & 0x7FFF)<<16;
  lo += hi>>15;
  if (lo > modulus) lo -= modulus;
  return lo;
}


int main(){

  setup();
	uint32_t random_num = get_random();
  uint32_t My_Public_Key = powModFast(g, random_num, p);

  uint32_t SecretKey;

  //tell you are client or server
  if (digitalRead(13) == 1) {
    //srver
    Serial.println("I am a server");
  	ServerState(My_Public_Key);
    //get the secretkey
    SecretKey = powModFast(ckey, random_num, p);
  }
  else {
    // client
    Serial.println("I am a client");
  	Clientstate(My_Public_Key);
    //get secretkey
    SecretKey = powModFast(skey, random_num, p);
  }

  Serial.println("Let's begin to chat");

	while(true){
    //you type and send to others
    //read when something are typed
    if (Serial.available()>0){
      char byteRead = Serial.read();
      //when type return, go next line
      if (byteRead == 13) {
        Serial.println();
      }
      //encrypt
      uint8_t EncryptedByte = byteRead ^ ((uint8_t)SecretKey);
      //send encrypted character to others
      Serial3.write(EncryptedByte);
      //change secretkey using next_key
      SecretKey = next_key(SecretKey);
      //print what you are typing
      Serial.print(byteRead);
    }

    //read other's character and print to your screen
    if (Serial3.available()>0){
      uint32_t byteRead3 = Serial3.read();
      //decrypt
      char DecryptedByte = byteRead3 ^ ((uint8_t)SecretKey);
      //when type return, go next line
      if (DecryptedByte == 13) {
        Serial.println();
      }
      //change secretkey using next_key
      SecretKey = next_key(SecretKey);
      //print what have been read
      Serial.print(DecryptedByte);
    }
  }

  Serial.end();
  Serial3.end();
  return 0;
}
