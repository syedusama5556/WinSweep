# ============================================================
# DELETE ANDROID / FLUTTER BUILD FOLDERS
# SAFE: Only deletes folders whose final name is exactly "build"
# ============================================================

$buildFolders = @(
    "C:\Users\syedu\AndroidStudioProjects\All_in__one_video_downloader(davzo_glez)\common\build"
    "C:\Users\syedu\AndroidStudioProjects\All_in__one_video_downloader(davzo_glez)\library\build"
    "C:\Users\syedu\AndroidStudioProjects\All_in__one_video_downloader\app\build"
    "C:\Users\syedu\AndroidStudioProjects\All_in__one_video_downloader\common\build"
    "C:\Users\syedu\AndroidStudioProjects\All_in__one_video_downloader\ffmpeg\build"
    "C:\Users\syedu\AndroidStudioProjects\All_in__one_video_downloader\library\build"
    "C:\Users\syedu\AndroidStudioProjects\Android-ChatGPT-API-Example\app\build"
    "C:\Users\syedu\AndroidStudioProjects\AnyaAIChatGPTAndroid\app\build"
    "C:\Users\syedu\AndroidStudioProjects\Browser_All_Video_Downloader\common\build"
    "C:\Users\syedu\AndroidStudioProjects\ChatWaifu_Mobile\Log\build"
    "C:\Users\syedu\AndroidStudioProjects\CloudVpnApp\app\build"
    "C:\Users\syedu\AndroidStudioProjects\CoinBit_Cryptocurrency_Tracker_kotlin\app\build"
    "C:\Users\syedu\AndroidStudioProjects\CustomObjectDetectionLiveFeedKotlin\app\build"
    "C:\Users\syedu\AndroidStudioProjects\Devsta\app\build"
    "C:\Users\syedu\AndroidStudioProjects\ESRGAN-Android-TFLite-Demo\app\build"
    "C:\Users\syedu\AndroidStudioProjects\Female_fitness_app\app\build"
    "C:\Users\syedu\AndroidStudioProjects\FudiNFC\app\build"
    "C:\Users\syedu\AndroidStudioProjects\FudiNFC\fudi-nfc\build"
    "C:\Users\syedu\AndroidStudioProjects\Google-Drive-API-Tutorial-in-Android\app\build"
    "C:\Users\syedu\AndroidStudioProjects\Hotel-Table-App\app\build"
    "C:\Users\syedu\AndroidStudioProjects\ImageCraft-Android\app\build"
    "C:\Users\syedu\AndroidStudioProjects\Instagram-Downloader\app-hilt\build"
    "C:\Users\syedu\AndroidStudioProjects\Instagram-Downloader\app\build"
    "C:\Users\syedu\AndroidStudioProjects\Instagram-Downloader\bubblenavigation\build"
    "C:\Users\syedu\AndroidStudioProjects\MyAiJourneyToMyOwnAssistentMaid\app\build"
    "C:\Users\syedu\AndroidStudioProjects\MyApplicationtest\app\build"
    "C:\Users\syedu\AndroidStudioProjects\MyTESTAPP\app\build"
    "C:\Users\syedu\AndroidStudioProjects\Object-Detection-Android-App\android_app\app\build"
    "C:\Users\syedu\AndroidStudioProjects\OuterTune_musicplayer\app\build"
    "C:\Users\syedu\AndroidStudioProjects\QRcodeScanner_android\app\build"
    "C:\Users\syedu\AndroidStudioProjects\RTranslator-real-time-translation-app\app\build"
    "C:\Users\syedu\AndroidStudioProjects\RTranslator-real-time-translation-app\switchbutton\build"
    "C:\Users\syedu\AndroidStudioProjects\RealSR-NCNN-Android-RealSR-NCNN-Android-GUI\app\build"
    "C:\Users\syedu\AndroidStudioProjects\Screen_Recorder_Ultimate_kotlin\app\build"
    "C:\Users\syedu\AndroidStudioProjects\Screen_Recorder_Ultimate_kotlin\capturescrn\build"
    "C:\Users\syedu\AndroidStudioProjects\Screen_Recorder_Ultimate_kotlin\fileutiltest\build"
    "C:\Users\syedu\AndroidStudioProjects\Screen_Recorder_Ultimate_kotlin\notificationssetup\build"
    "C:\Users\syedu\AndroidStudioProjects\Screen_Recorder_Ultimate_kotlin\uifiletm\build"
    "C:\Users\syedu\AndroidStudioProjects\Screen_Recorder_Ultimate_kotlin\utilcmnuse\build"
    "C:\Users\syedu\AndroidStudioProjects\SuperVPN\app\build"
    "C:\Users\syedu\AndroidStudioProjects\SuperVPN\vpnLib\build"
    "C:\Users\syedu\AndroidStudioProjects\ZUPeshawarClone\app\build"
    "C:\Users\syedu\AndroidStudioProjects\nfc-card-reader\app\build"
    "C:\Users\syedu\AndroidStudioProjects\onnxruntime-whisper-local-android\app\build"
    "C:\Users\syedu\AndroidStudioProjects\pocketsphinx-android-demo\app\build"
    "C:\Users\syedu\AndroidStudioProjects\pocketsphinx-android-demo\models\build"
    "C:\Users\syedu\AndroidStudioProjects\pocketsphinx-android-demo\wear\build"
    "C:\Users\syedu\StudioProjects\reposta_6_1\app\build"
    "C:\Users\syedu\Videos\Captures\RealSR-NCNN-Androidv\RealSR-NCNN-Android-CLI\app\build"

    "E:\reskin\All-Reskin-AVD-Android\AIOSS_AdmobAds_manbij\nativetemplates\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_247downloader_new2\app\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_247downloader_new2\common\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_247downloader_new2\ffmpeg\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_247downloader_new2\library\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_ARSHADEHRAR\app\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_ARSHADEHRAR\common\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_ARSHADEHRAR\ffmpeg\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_ARSHADEHRAR\library\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_AceThinker_videomate\common\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_AceThinker_videomate\ffmpeg\build"
    "E:\reskin\All-Reskin-AVD-Android\All_in__one_video_downloader_AceThinker_videomate\library\build"
    "E:\reskin\All_in__one_video_downloader(acethinker_video_downloader_newui_design)\common\build"
    "E:\reskin\All_in__one_video_downloader(acethinker_video_downloader_newui_design)\ffmpeg\build"
    "E:\reskin\All_in__one_video_downloader(acethinker_video_downloader_newui_design)\library\build"
    "E:\reskin\All_in__one_video_downloader(ivratamuzadde862@gmail.com)\app\build"
    "E:\reskin\All_in__one_video_downloader(ivratamuzadde862@gmail.com)\common\build"
    "E:\reskin\All_in__one_video_downloader(ivratamuzadde862@gmail.com)\ffmpeg\build"
    "E:\reskin\All_in__one_video_downloader(ivratamuzadde862@gmail.com)\library\build"
    "E:\reskin\All_in__one_video_downloader(rajubhaichitrodavala) (8th,Aug,2024)\app\build"
    "E:\reskin\All_in__one_video_downloader(rajubhaichitrodavala) (8th,Aug,2024)\common\build"
    "E:\reskin\All_in__one_video_downloader(rajubhaichitrodavala) (8th,Aug,2024)\ffmpeg\build"
    "E:\reskin\All_in__one_video_downloader(rajubhaichitrodavala) (8th,Aug,2024)\library\build"
    "E:\reskin\All_in__one_video_downloader(rajubhaichitrodavala)\app\build"
    "E:\reskin\All_in__one_video_downloader(rajubhaichitrodavala)\common\build"
    "E:\reskin\All_in__one_video_downloader(rajubhaichitrodavala)\ffmpeg\build"
    "E:\reskin\All_in__one_video_downloader(rajubhaichitrodavala)\library\build"
    "E:\reskin\All_in__one_video_downloader_Durgesh_Nayak_update_21_aug\app\build"
    "E:\reskin\All_in__one_video_downloader_Durgesh_Nayak_update_21_aug\common\build"
    "E:\reskin\All_in__one_video_downloader_Durgesh_Nayak_update_21_aug\ffmpeg\build"
    "E:\reskin\All_in__one_video_downloader_Durgesh_Nayak_update_21_aug\library\build"
    "E:\reskin\All_in__one_video_downloader_Durgesh_Nayak_update_old\app\build"
    "E:\reskin\All_in__one_video_downloader_Durgesh_Nayak_update_old\common\build"
    "E:\reskin\All_in__one_video_downloader_Durgesh_Nayak_update_old\ffmpeg\build"
    "E:\reskin\All_in__one_video_downloader_Durgesh_Nayak_update_old\library\build"
    "E:\reskin\All_in__one_video_downloader_Surendra_Saini\app\build"
    "E:\reskin\Fast_Video_Downloader_Usama_Bhatti\common\build"
    "E:\reskin\Fast_Video_Downloader_Usama_Bhatti\ffmpeg\build"
    "E:\reskin\Fast_Video_Downloader_Usama_Bhatti\library\build"
    "E:\reskin\Insta_Tools_ARSHADEHRAR\app\build"
    "E:\reskin\Insta_Tools_ARSHADEHRAR\in_app_purchase\build"
    "E:\reskin\Insta_Tools_ARSHADEHRAR\nativeads\build"
    "E:\reskin\Insta_Tools_ARSHADEHRAR\ucrop\build"
    "E:\reskin\SaveVideoStatus(fastars999@gmail.com)\jzvd\build"
    "E:\reskin\TFLite-Object-Detection-Android-App-Tutorial-Using-YOLOv5\app\build"
    "E:\reskin\UZR-STUFF-Playstore-Apps\VideoMakerPro\library\build"
    "E:\reskin\UZR-STUFF-Playstore-Apps\prestige_vendorapp\android\build"
    "E:\reskin\UZR-STUFF-Playstore-Apps\will_aibot\LuckyWheel\build"
    "E:\reskin\UZR-STUFF-Playstore-Apps\will_aibot\app\build"
    "E:\reskin\VideoDownloaderOLD\app\build"
    "E:\reskin\VideoDownloaderOLD\common\build"
    "E:\reskin\VideoDownloaderOLD\ffmpeg\build"
    "E:\reskin\VideoDownloaderOLD\library\build"
    "E:\reskin\wireguard-android\tunnel\build"
    "E:\reskin\wireguard-android\ui\build"
    "E:\xampp\htdocs\kohat-food-web-flutter\kohat_food_rider_app\android\build"
    "E:\xampp\htdocs\kohat-food-web-flutter\kohat_food_vendor_app\android\build"
    "E:\xampp\htdocs\mobile_device_management_tool_reactjs_android\android_app\app\build"
    "E:\xampp\htdocs\phoenix_android_botnet\apk\mmm\app\build"
    "E:\xampp\htdocs\react_native_test_app\android\build"
)

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " BUILD FOLDER DELETION" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

$valid = @()
$missing = @()
$blocked = @()

foreach ($path in $buildFolders) {

    # SAFETY CHECK 1:
    # Must literally end with \build
    if ([System.IO.Path]::GetFileName($path.TrimEnd('\')) -ne "build") {
        $blocked += $path
        continue
    }

    # SAFETY CHECK 2:
    # Must exist and actually be a directory
    $item = Get-Item -LiteralPath $path -ErrorAction SilentlyContinue

    if ($null -eq $item) {
        $missing += $path
        continue
    }

    if (-not $item.PSIsContainer) {
        $blocked += $path
        continue
    }

    $valid += $path
}

Write-Host "Build folders found : $($valid.Count)" -ForegroundColor Green
Write-Host "Already missing     : $($missing.Count)" -ForegroundColor Yellow
Write-Host "Blocked by safety   : $($blocked.Count)" -ForegroundColor Red
Write-Host ""

if ($blocked.Count -gt 0) {
    Write-Host "SAFETY BLOCKED:" -ForegroundColor Red
    $blocked | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    Write-Host ""
}

if ($valid.Count -eq 0) {
    Write-Host "Nothing to delete." -ForegroundColor Yellow
    exit
}

Write-Host "The following folders WILL be deleted:" -ForegroundColor Yellow
Write-Host ""

$valid | ForEach-Object {
    Write-Host "  $_" -ForegroundColor Gray
}

Write-Host ""
Write-Host "IMPORTANT: Only folders named 'build' will be deleted." -ForegroundColor Cyan
Write-Host ""

$confirmation = Read-Host "Type DELETE to continue"

if ($confirmation -ne "DELETE") {
    Write-Host ""
    Write-Host "Cancelled. Nothing was deleted." -ForegroundColor Yellow
    exit
}

Write-Host ""
Write-Host "Deleting..." -ForegroundColor Cyan
Write-Host ""

$deleted = 0
$failed = 0

foreach ($path in $valid) {

    try {
        Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction Stop

        Write-Host "[DELETED] $path" -ForegroundColor Green
        $deleted++
    }
    catch {
        Write-Host "[FAILED]  $path" -ForegroundColor Red
        Write-Host "          $($_.Exception.Message)" -ForegroundColor DarkRed
        $failed++
    }
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " COMPLETE" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Deleted : $deleted" -ForegroundColor Green
Write-Host "Failed  : $failed" -ForegroundColor Red
Write-Host "Missing : $($missing.Count)" -ForegroundColor Yellow
Write-Host ""