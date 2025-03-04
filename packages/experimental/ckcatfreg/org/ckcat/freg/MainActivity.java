package org.ckcat.freg;

import android.app.Activity;
import android.os.ServiceManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.IFregService;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;

public class MainActivity extends Activity implements OnClickListener{
	private final static String TAG = "CKCatFreg";
	
	private IFregService fregService = null;
	private EditText valueText = null;
	private Button readButton = null;
	private Button writeButton = null;
	private Button clearButton = null;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		
		
		
		try {
			// //获得ServiceManager类
			// Class<?> ServiceManager = Class.forName("android.os.ServiceManager");
			
			// //获得ServiceManager的getService方法
			// Method getService = ServiceManager.getMethod("getService", String.class);
			 
			// //调用getService获取RemoteService
			// IBinder oRemoteService = (IBinder) getService.invoke(null,"ckcatfreg");
			
			fregService = IFregService.Stub.asInterface(ServiceManager.getService("ckcatfreg"));
			
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		 


	
		valueText = (EditText) findViewById(R.id.edit_content);
		readButton = (Button) findViewById(R.id.button_read);
		writeButton = (Button) findViewById(R.id.button_write);
		clearButton = (Button) findViewById(R.id.button_clear);
		
		readButton.setOnClickListener(this);
		writeButton.setOnClickListener(this);
		clearButton.setOnClickListener(this);
		
		Log.i(TAG, "CKCatFreg Activity Created.");
	}
	
	@Override
	public void onClick(View v) {
		// TODO Auto-generated method stub
		if (v.equals(readButton)) {
			try {
				int val = fregService.getVal();
				String text = String.valueOf(val);
				valueText.setText(text);
			} catch (Exception e) {
				Log.e(TAG, "Remote Exception while reading value to freg service.");
			}
		}else if (v.equals(writeButton)) {
			try {
				String text = valueText.getText().toString();
				int val = Integer.parseInt(text);
				fregService.setVal(val);
			} catch (Exception e) {
				Log.e(TAG, "Remote Exception while writing value to freg service.");
			}
		}else if (v.equals(clearButton)) {
				String text = "";
				valueText.setText(text);
		}
	}
}
