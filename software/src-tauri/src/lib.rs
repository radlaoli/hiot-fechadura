use serde::Serialize;
#[derive(Serialize)]
struct TagData {
    id: String,
    kind: String,
}

#[tauri::command]
async fn scan_rfid(app: tauri::AppHandle) -> Result<TagData, String> {
    #[cfg(mobile)]
    {
        use tauri_plugin_nfc::NfcExt;
        let nfc = app.nfc();

        if !nfc.is_available().map_err(|e| e.to_string())? {
            return Err("NFC is not available on this device".to_string());
        }

        let scan_result = nfc
            .scan(tauri_plugin_nfc::ScanRequest {
                kind: tauri_plugin_nfc::ScanKind::Tag {
                    uri: None,
                    mime_type: None,
                },
                keep_session_alive: false,
            })
            .map_err(|e| e.to_string())?;

        let tag = TagData {
            id: format_tag_id(&scan_result.id),
            kind: scan_result.kind.join(", "),
        };

        println!("Tag scanned in Rust: id={}, kind={}", tag.id, tag.kind);

        Ok(tag)
    }
    #[cfg(not(mobile))]
    {
        let _ = app;
        Err(
            "RFID/NFC scanning is currently only supported on mobile devices in this app."
                .to_string(),
        )
    }
}

#[cfg(mobile)]
fn format_tag_id(id: &[u8]) -> String {
    id.iter()
        .map(|byte| format!("{byte:02X}"))
        .collect::<Vec<_>>()
        .join(":")
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .setup(|app| {
            #[cfg(mobile)]
            app.handle().plugin(tauri_plugin_nfc::init())?;

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![scan_rfid])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
