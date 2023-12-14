package es.chiteroman.safetynetfix;

import android.os.Build;
import android.util.Log;

import java.lang.reflect.Field;
import java.security.Provider;
import java.security.Security;
import java.util.HashMap;
import java.util.Map;

public final class EntryPoint {
    private static final Map<String, String> map = new HashMap<>();

    static {
        map.put("PRODUCT", "WW_Z01H");
        map.put("DEVICE", "ASUS_Z01H_1");
        map.put("MANUFACTURER", "asus");
        map.put("BRAND", "asus");
        map.put("MODEL", "ASUS_Z01HD");
        map.put("FINGERPRINT", "asus/WW_Z01H/ASUS_Z01H_1:7.1.1/NMF26F/WW_user_71.60.139.30_20180306:user/release-keys");
    }

    public static void init() {
        spoofDevice();

        try {
            Provider provider = Security.getProvider("AndroidKeyStore");

            Provider customProvider = new CustomProvider(provider);

            Security.removeProvider("AndroidKeyStore");
            Security.insertProviderAt(customProvider, 1);

            LOG("Spoof KeyStoreSpi and Provider done!");

        } catch (Exception e) {
            LOG("ERROR: " + e);
        }
    }

    static void spoofDevice() {
        map.forEach(EntryPoint::setProp);
    }

    private static void setProp(String name, String value) {
        try {
            Field field = Build.class.getDeclaredField(name);
            field.setAccessible(true);
            String oldValue = (String) field.get(null);
            field.set(null, value);
            field.setAccessible(false);
            if (value.equals(oldValue)) return;
            LOG(String.format("Field: '%s' with value '%s' is now set to '%s'", name, oldValue, value));
        } catch (Exception e) {
            LOG("setprop exception: " + e);
        }
    }

    static void LOG(String msg) {
        Log.d("SNFix/Java", msg);
    }
}