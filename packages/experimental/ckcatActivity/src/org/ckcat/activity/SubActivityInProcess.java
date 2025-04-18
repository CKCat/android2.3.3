package org.ckcat.activity;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;

public class SubActivityInProcess extends Activity implements View.OnClickListener {
    private static final String TAG = "SubActivityInProcess";
    private Button finishBtn = null;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.sub);
        finishBtn = (Button)findViewById(R.id.btn_finish);
        finishBtn.setOnClickListener(this);
        Log.i(TAG, "Sub Activity In Process Created.");
    }

    @Override
    public void onClick(View view) {
        if(view.equals(finishBtn)){
            finish();
        }
    }
}
