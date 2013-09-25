package com.java.javagree;

import android.app.Application;

public class App extends Application {

	public void onCreate() {
              super.onCreate();
        }

        private static MainActivity currentActivity = null;

        static void setCurrentActivity(MainActivity currentActivity){
              App.currentActivity = currentActivity;
        }

        static MainActivity getCurrentActivity(){
            return App.currentActivity;
      }
}