package org.ckcat.activity;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;

public class MainActivity extends Activity implements View.OnClickListener{
    private static final String TAG = "MainActivity";
    private Button startInprocessButton = null;
    private Button startInNewProcessButton = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        startInprocessButton = (Button)findViewById(R.id.btn_start_in_process);
        startInNewProcessButton = (Button)findViewById(R.id.btn_start_in_new_process);
        startInprocessButton.setOnClickListener(this);
        startInNewProcessButton.setOnClickListener(this);
        Log.i(TAG, "onCreate: MainActivity created.");
    }

    @Override
    public void onClick(View view) {
        if (view.equals(startInprocessButton)){
            Intent intent = new Intent("org.ckcat.activity.subactivity.in.process");
            startActivity(intent);
        }else if (view.equals(startInNewProcessButton)){
            Intent intent = new Intent("org.ckcat.activity.subactivity.in.new.process");
            startActivity(intent);
        }
    }
}
