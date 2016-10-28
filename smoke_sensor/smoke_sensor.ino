void setup() {
  Serial.begin(115200);

}

void loop() 
{
  uint16_t smoke_adc = analogRead(A0);
  Serial.print("ADC read: ");
  Serial.println(smoke_adc);
  delay(100);
}
