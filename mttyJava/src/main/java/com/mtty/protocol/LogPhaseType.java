/**
 * Autogenerated by Avro
 * 
 * DO NOT EDIT DIRECTLY
 */
package com.mtty.protocol;  
@SuppressWarnings("all")
@org.apache.avro.specific.AvroGenerated
public enum LogPhaseType { 
  BID, SHOW, VIEW, CLICK, TRACK, MAPPING  ;
  public static final org.apache.avro.Schema SCHEMA$ = new org.apache.avro.Schema.Parser().parse("{\"type\":\"enum\",\"name\":\"LogPhaseType\",\"namespace\":\"com.mtty.protocol\",\"symbols\":[\"BID\",\"SHOW\",\"VIEW\",\"CLICK\",\"TRACK\",\"MAPPING\"]}");
  public static org.apache.avro.Schema getClassSchema() { return SCHEMA$; }
}