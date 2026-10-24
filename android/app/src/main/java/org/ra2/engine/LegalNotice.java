package org.ra2.engine;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/** Local confirmation only: never proof of ownership or a transfer of rights. */
final class LegalNotice {
    private static final int VERSION = 1;
    private static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences("legal_notice", Context.MODE_PRIVATE);
    }
    static boolean accepted(Context c) { return prefs(c).getInt("version",0)==VERSION; }
    private static String asset(Context c,String name) throws Exception {
        try(InputStream in=c.getAssets().open(name);ByteArrayOutputStream out=new ByteArrayOutputStream()) {
            byte[] b=new byte[8192];int n;
            while((n=in.read(b))!=-1)out.write(b,0,n);
            return new String(out.toByteArray(),StandardCharsets.UTF_8);
        }
    }
    private static ScrollView text(Activity a,String text) {
        ScrollView scroll=new ScrollView(a);
        TextView view=new TextView(a);view.setText(text);view.setTextSize(16);
        int pad=Math.round(16*a.getResources().getDisplayMetrics().density);
        view.setPadding(pad,pad,pad,pad);view.setTextIsSelectable(true);
        scroll.addView(view);return scroll;
    }
    static void show(Activity a,Runnable after) {
        final String terms;
        try { terms=asset(a,"TERMS.txt"); }
        catch(Exception e) {
            new AlertDialog.Builder(a).setTitle("Terms unavailable")
                .setMessage("The bundled terms could not be read. Please reinstall this build.")
                .setPositiveButton("Exit",(d,w)->a.finish()).setCancelable(false).show();return;
        }
        LinearLayout panel=new LinearLayout(a);panel.setOrientation(LinearLayout.VERTICAL);
        panel.addView(text(a,terms),new LinearLayout.LayoutParams(-1,0,1));
        CheckBox rights=new CheckBox(a);rights.setChecked(false);
        rights.setText("I agree to the terms and confirm I have the rights to use the game files I select.");
        rights.setMinHeight(Math.round(56*a.getResources().getDisplayMetrics().density));
        panel.addView(rights);
        AlertDialog dialog=new AlertDialog.Builder(a).setTitle("Terms & game files")
            .setView(panel).setPositiveButton("Continue",null)
            .setNegativeButton("Decline",(d,w)->a.finish()).setCancelable(false).create();
        dialog.setOnShowListener(d->{
            android.widget.Button go=dialog.getButton(AlertDialog.BUTTON_POSITIVE);
            go.setEnabled(false);rights.setOnCheckedChangeListener((b,checked)->go.setEnabled(checked));
            go.setOnClickListener(v->{
                if(!rights.isChecked())return;
                // Commit before importing or starting: both activities use separate processes.
                if(!prefs(a).edit().putInt("version",VERSION)
                    .putLong("accepted_at_ms",System.currentTimeMillis()).commit()) {
                    Toast.makeText(a,"Could not save confirmation. Please try again.",Toast.LENGTH_LONG).show();return;
                }
                dialog.dismiss();if(after!=null)after.run();
            });
            dialog.getWindow().setLayout(-1,Math.round(a.getResources().getDisplayMetrics().heightPixels*0.92f));
        });
        dialog.show();
    }
    static void credits(Activity a) {
        StringBuilder all=new StringBuilder();
        try {
            for(String file:new String[]{"CREDITS.md","LICENSE","THIRD_PARTY_NOTICES.md","LIBCXX-LICENSE.txt"})
                all.append(file).append("\n\n").append(asset(a,file)).append("\n\n");
        } catch(Exception e) { all.append("Some bundled notices could not be read. See the source repository for licenses."); }
        new AlertDialog.Builder(a).setTitle("Credits & licenses").setView(text(a,all.toString()))
            .setPositiveButton("Close",null).show();
    }
}
