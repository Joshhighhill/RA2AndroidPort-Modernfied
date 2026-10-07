package org.ra2.engine;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * W2-AR-03 / W2-AR-04: the launcher activity. It checks that the user's own
 * Red Alert 2 files are in the app's external files dir
 * (Android/data/org.ra2.engine/files/ra2/, the folder the engine reads) and
 * starts the game when they are. Otherwise it explains what is missing and
 * offers an in-app import: the Storage Access Framework folder picker
 * (ACTION_OPEN_DOCUMENT_TREE -- works on Android 5 to 15, no storage
 * permission) copies the files in under their canonical names. That replaces
 * the old "adb push into Android/data" recipe, which Android 11+ refuses for
 * the shell user and file managers.
 *
 * Runs in its own process (":setup", see the manifest) so the engine's
 * exit() at the end of main() cannot take it down when the engine hands back
 * to it (the missing-files page's IMPORT button).
 */
public class SetupActivity extends Activity {
    private static final String TAG = "RA2Setup";
    private static final int REQ_TREE = 1;
    public static final String EXTRA_IMPORT = "org.ra2.engine.IMPORT";

    /** The files the engine reads; the first four are required (engine/data_dir.inc). */
    static final String[] FILES = {
        "ra2.mix", "ra2md.mix", "language.mix", "multimd.mix",
        "langmd.mix", "mapsmd03.mix", "theme.mix", "expand01.mix",
        "Maps01.mix", "MAPS02.MIX", "ra2.csf",
    };
    static final int REQUIRED = 4;

    private TextView status;
    private ProgressBar progress;
    private Button importButton, startButton;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private volatile boolean copying = false;

    static File dataDir(Activity a) {
        File ext = a.getExternalFilesDir(null);
        File d = new File(ext != null ? ext : a.getFilesDir(), "ra2");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();   // visible over MTP even before the first import
        return d;
    }

    static List<String> missing(File dir, boolean requiredOnly) {
        List<String> m = new ArrayList<>();
        for (int i = 0; i < (requiredOnly ? REQUIRED : FILES.length); ++i) {
            File f = new File(dir, FILES[i]);
            if (!f.isFile() || f.length() == 0) m.add(FILES[i]);
        }
        return m;
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        File dir = dataDir(this);
        boolean forceImport = getIntent() != null && getIntent().getBooleanExtra(EXTRA_IMPORT, false);
        if (!forceImport && missing(dir, true).isEmpty()) {
            startGame();
            return;
        }
        buildUi();
        refresh();
    }

    private int dp(int v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
    }

    private Button button(String label, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(label);
        b.setMinHeight(dp(56));   // >= 48 dp touch target
        b.setOnClickListener(l);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lp.setMargins(dp(6), dp(6), dp(6), dp(6));
        b.setLayoutParams(lp);
        return b;
    }

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.BLACK);
        root.setPadding(dp(24), dp(16), dp(24), dp(16));

        TextView title = new TextView(this);
        title.setText(R.string.setup_title);
        title.setTextColor(Color.rgb(255, 200, 90));
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
        root.addView(title);

        ScrollView scroll = new ScrollView(this);
        status = new TextView(this);
        status.setTextColor(Color.rgb(220, 220, 220));
        status.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        status.setPadding(0, dp(8), 0, dp(8));
        scroll.addView(status);
        root.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setVisibility(View.GONE);
        root.addView(progress);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER);
        importButton = button(getString(R.string.setup_import), v -> pickFolder());
        startButton = button(getString(R.string.setup_start), v -> startGame());
        Button exit = button(getString(R.string.setup_exit), v -> finish());
        row.addView(importButton);
        row.addView(startButton);
        row.addView(exit);
        root.addView(row);
        setContentView(root);
    }

    private void refresh() {
        File dir = dataDir(this);
        List<String> req = missing(dir, true);
        List<String> all = missing(dir, false);
        StringBuilder sb = new StringBuilder();
        sb.append(getString(R.string.setup_intro)).append("\n\n");
        sb.append(getString(R.string.setup_folder)).append("\n  ").append(dir.getAbsolutePath()).append("\n\n");
        if (req.isEmpty()) {
            sb.append(getString(R.string.setup_ready)).append("\n");
        } else {
            sb.append(getString(R.string.setup_missing_required)).append("\n");
            for (String f : req) sb.append("  ").append(f).append("\n");
        }
        List<String> opt = new ArrayList<>(all);
        opt.removeAll(req);
        if (!opt.isEmpty()) {
            sb.append("\n").append(getString(R.string.setup_missing_optional)).append("\n");
            for (String f : opt) sb.append("  ").append(f).append("\n");
        }
        sb.append("\n").append(getString(R.string.setup_how));
        status.setText(sb.toString());
        startButton.setEnabled(req.isEmpty() && !copying);
        importButton.setEnabled(!copying);
    }

    private void pickFolder() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivityForResult(i, REQ_TREE);
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != REQ_TREE || res != RESULT_OK || data == null || data.getData() == null) return;
        final Uri tree = data.getData();
        copying = true;
        progress.setVisibility(View.VISIBLE);
        progress.setProgress(0);
        refresh();
        new Thread(() -> importFrom(tree), "ra2-import").start();
    }

    /** Copies every known file found in the picked folder (case-insensitive match). */
    private void importFrom(Uri tree) {
        ContentResolver cr = getContentResolver();
        File dir = dataDir(this);
        Map<String, String> canon = new HashMap<>();
        for (String f : FILES) canon.put(f.toLowerCase(Locale.ROOT), f);
        Map<String, Uri> found = new HashMap<>();
        Map<String, Long> sizes = new HashMap<>();
        long total = 0;
        String error = null;
        try {
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree,
                    DocumentsContract.getTreeDocumentId(tree));
            try (Cursor c = cr.query(children, new String[]{
                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_SIZE}, null, null, null)) {
                while (c != null && c.moveToNext()) {
                    String name = c.getString(1);
                    String key = name == null ? null : canon.get(name.toLowerCase(Locale.ROOT));
                    if (key == null) continue;
                    found.put(key, DocumentsContract.buildDocumentUriUsingTree(tree, c.getString(0)));
                    long sz = c.isNull(2) ? 0 : c.getLong(2);
                    sizes.put(key, sz);
                    total += sz;
                }
            }
            long done = 0;
            byte[] buf = new byte[1 << 20];
            for (Map.Entry<String, Uri> e : found.entrySet()) {
                File out = new File(dir, e.getKey());
                File tmp = new File(dir, e.getKey() + ".part");
                final String name = e.getKey();
                ui.post(() -> status.setText(getString(R.string.setup_copying, name)));
                try (InputStream in = cr.openInputStream(e.getValue());
                     OutputStream os = new FileOutputStream(tmp)) {
                    if (in == null) throw new java.io.IOException("cannot open " + name);
                    int n;
                    while ((n = in.read(buf)) > 0) {
                        os.write(buf, 0, n);
                        done += n;
                        final int p = total > 0 ? (int) (done * 1000 / total) : 0;
                        ui.post(() -> progress.setProgress(p));
                    }
                }
                Long want = sizes.get(name);
                if (want != null && want > 0 && tmp.length() != want)
                    throw new java.io.IOException(name + ": copied " + tmp.length() + " of " + want + " bytes");
                //noinspection ResultOfMethodCallIgnored
                out.delete();
                if (!tmp.renameTo(out)) throw new java.io.IOException("cannot rename " + tmp);
                Log.i(TAG, "imported " + name + " (" + out.length() + " bytes)");
            }
            Log.i(TAG, "import done: " + found.size() + " files, " + total + " bytes");
        } catch (Exception ex) {
            Log.e(TAG, "import failed", ex);
            error = ex.getMessage();
        }
        final String err = error;
        final int count = found.size();
        ui.post(() -> {
            copying = false;
            progress.setVisibility(View.GONE);
            refresh();
            String head = err != null ? getString(R.string.setup_failed, err)
                                      : getString(R.string.setup_imported, count);
            status.setText(head + "\n\n" + status.getText());
            if (err == null && missing(dataDir(this), true).isEmpty()) startGame();
        });
    }

    private void startGame() {
        // NEW_TASK only: RA2Activity is singleInstance, so a running game
        // (backgrounded with Home) is brought back as it was, not restarted.
        Intent i = new Intent(this, RA2Activity.class);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(i);
        finish();
    }
}
