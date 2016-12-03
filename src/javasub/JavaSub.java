/**
 * our dsetination is /mnt/hgfs/ShareFolder/ffmTest/out, so
 * cp -f JavaSub/dist/JavaSub.jar .
 * java -jar JavaSub.jar
 *
 * # get ffmTest log:
 * tail -f log.log
 */
package javasub;

public class JavaSub {

    public static void main(String[] args) {
        MqttSub mqttSub = new MqttSub();
        mqttSub.Connect(); 
    }

}
