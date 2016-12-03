/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package javasub;

import com.google.gson.Gson;
import java.lang.management.ManagementFactory;
import java.text.SimpleDateFormat;
import java.util.Date;
import org.fusesource.hawtbuf.Buffer;
import org.fusesource.hawtbuf.UTF8Buffer;
import org.fusesource.mqtt.client.Callback;
import org.fusesource.mqtt.client.CallbackConnection;
import org.fusesource.mqtt.client.Listener;
import org.fusesource.mqtt.client.MQTT;
import org.fusesource.mqtt.client.QoS;
import org.fusesource.mqtt.client.Topic;
import sun.misc.Lock;
import sun.misc.Signal;
import sun.misc.SignalHandler;



/**
 *
 * @author gavin
 */
public class MqttSub implements SignalHandler{

    private MQTT mqtt;
    private CallbackConnection connection;
    Topic[] topics = {new Topic("/1234/videoinfoAsk", QoS.AT_MOST_ONCE)};
    private String msg;
    private boolean ffmTestIsPushing;
    private Process process;
    private String CurrentUrl;
    private static Lock lock = new Lock();
    
    public MqttSub() {
        
        ffmTestIsPushing = false;        
        
    }

    @Override
    public void handle(Signal signal) {        
        System.out.println("receive " + signal);        
        lock.unlock();
    }
    
    public void Connect() {
        try {
            MqttSub mqttSubHandler = new MqttSub();
            Signal.handle(new Signal("TERM"), mqttSubHandler);
            Signal.handle(new Signal("INT"), mqttSubHandler);
            
            mqtt = new MQTT();
            
            
            mqtt.setHost("tcp://120.27.188.84:1883");
            mqtt.setClientId("Java Subscriber");
            mqtt.setKeepAlive((short) 20);
            mqtt.setUserName("easydarwin");
            mqtt.setPassword("123456");
            mqtt.setVersion("3.1.1");

            connection = mqtt.callbackConnection();
            connection.listener(listener);
            connection.connect(ConnectCB);

            lock.lock();
            //SimpleDateFormat df = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
            System.out.println(new Date() + " Subscriber started. " + ManagementFactory.getRuntimeMXBean().getName());
            
            // expect block here. until receive SIGTERM
            lock.lock();            
        } catch (Exception ex) {
            ex.printStackTrace();
        } finally {            
            connection.disconnect(DisconnectCB);
            if (null != process && process.isAlive()){
                process.destroy();
                ffmTestIsPushing = false;             
                System.out.println("ffmTest stopped.\n");
            }
            
            System.out.println("Connect done.");          
        }

    }

    private Listener listener = new Listener() {
        @Override
        public void onConnected() {
            System.out.println("listener.onConnected");
        }

        @Override
        public void onDisconnected() {
            System.out.println("listener.onDisconnected");
        }

        @Override
        public void onPublish(UTF8Buffer utfb, Buffer buffer, Runnable r) {   
            msg = new String(buffer.toByteArray());
            //System.out.println(msg + '\n');
            r.run();
            ParseMQThenSendCmd();
        }

        @Override
        public void onFailure(Throwable thrwbl) {
            System.out.println("listener.onFailure");
        }
    };
    private Callback<Void> ConnectCB = new Callback<Void>() {
        @Override
        public void onSuccess(Void t) {
            System.out.println("connect.onSuccess");
            connection.subscribe(topics, new Callback<byte[]>() {
                @Override
                public void onSuccess(byte[] t) {
                    System.out.println("subscribe.onSuccess");
                }

                @Override
                public void onFailure(Throwable thrwbl) {
                    System.out.println("subscribe.onFailure");
                }
            });
        }

        @Override
        public void onFailure(Throwable thrwbl) {
            System.out.println("connect.onFailure: " + thrwbl.getMessage());
            thrwbl.printStackTrace();
        }
    };

    private Callback<Void> DisconnectCB = new Callback<Void>() {
        @Override
        public void onSuccess(Void t) {
            System.out.println("disconnect.onSuccess");
        }

        @Override
        public void onFailure(Throwable thrwbl) {
            System.out.println("disconnect.onFailure");
        }
    };
    
    private void ParseMQThenSendCmd(){
        PushRequest pushRequest = new Gson().fromJson(msg, PushRequest.class);
        //System.out.println("pushRequest.URL: " + pushRequest.URL + " CurrentUrl: " + CurrentUrl);
        if (pushRequest.Operation.equalsIgnoreCase("Stop") 
                && ffmTestIsPushing 
                && pushRequest.URL.equalsIgnoreCase(CurrentUrl)
                && null != process && process.isAlive() ){
            // send signal
            try {
                process.destroy();
                ffmTestIsPushing = false;
                System.out.println("ffmTest stopped.\n");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        else if(pushRequest.Operation.equalsIgnoreCase("Begin") && !ffmTestIsPushing){
            // start ffmtest
            try {
                // rtsp://120.27.188.84:8888/realtime/$1234/1/realtime.sdp
                // because $ will be regared as Escape character, so add \ before $ to tell shell that
                // $ is a character not a Escape character.
                // test in leapmotor win7, java1.8, ubuntu 16.04, $ is regared as $.
                
                //String cmd = "../ffmTest " + pushRequest.URL.replace("$", "\\$");
                String cmd = "../ffmTest " + pushRequest.URL;
                System.out.println("start ffmTest: " + cmd + '\n');
                process = Runtime.getRuntime().exec(cmd);
                System.out.println(new Date() + " ffmTest started.");
                ffmTestIsPushing = true;
                CurrentUrl = pushRequest.URL;
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }


}
