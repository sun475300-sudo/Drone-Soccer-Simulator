using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

// Distribution launcher. No PowerShell execution policy, Python install, Git or UE install needed.
internal static class PortableLauncher
{
    static readonly JavaScriptSerializer Json = new JavaScriptSerializer();
    static readonly string Root = AppDomain.CurrentDomain.BaseDirectory;
    static string Data = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "ImperialWingsSimulator", "Saved");
    static readonly string Game = Path.Combine(Root,"Windows","ImperialWingsSimulator.exe");
    static bool RuntimeInstalled()
    {
        using(var baseKey=RegistryKey.OpenBaseKey(RegistryHive.LocalMachine,RegistryView.Registry64))
        using(var key=baseKey.OpenSubKey(@"SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64"))
        {
            if(key==null || Convert.ToInt32(key.GetValue("Installed",0))!=1)return false;
            int major=Convert.ToInt32(key.GetValue("Major",0)),minor=Convert.ToInt32(key.GetValue("Minor",0));
            return major>14 || major==14 && minor>=44;
        }
    }
    static string Quote(string value)
    {
        // Windows CommandLineToArgvW quoting, including a trailing backslash.
        var output=new System.Text.StringBuilder("\"");int slashes=0;
        foreach(char c in value){if(c=='\\'){slashes++;continue;}if(c=='"'){output.Append('\\',slashes*2+1);output.Append(c);}else{output.Append('\\',slashes);output.Append(c);}slashes=0;}
        output.Append('\\',slashes*2);return output.Append('"').ToString();
    }
    static string ExplicitDirectory(string[] args, string option)
    {
        int index=Array.IndexOf(args,option);
        if(index<0)return null;
        if(index+1>=args.Length)throw new ArgumentException(option+" requires an absolute directory.");
        string value=args[index+1];
        if(value.Length<3 || value[1]!=':' || (value[2]!='\\' && value[2]!='/'))
            throw new ArgumentException(option+" requires an absolute local drive path.");
        return Path.GetFullPath(value);
    }
    [STAThread] static int Main(string[] args)
    {
        Process bridge=null;string log=null;
        try
        {
            // Explicit per-process QA/portable paths; the normal user data default is retained.
            string explicitData=ExplicitDirectory(args,"--data-dir");
            string engineUserDirectory=ExplicitDirectory(args,"--engine-user-dir");
            if(explicitData!=null)Data=explicitData;
            int inspectionIndex=Array.IndexOf(args,"--inspect");
            if(inspectionIndex>=0)
            {
                if(inspectionIndex+1>=args.Length)throw new ArgumentException("--inspect requires a result path.");
                var state=new {windows64=Environment.Is64BitOperatingSystem,game_exists=File.Exists(Game),vc_runtime_installed=RuntimeInstalled(),
                    bridge_exists=File.Exists(Path.Combine(Root,"Guidance","ImperialGuidance.exe")),data_directory=Data,game=Game,
                    requires_installed_python=false,requires_installed_git=false,requires_installed_unreal=false};
                File.WriteAllText(args[inspectionIndex+1],Json.Serialize(state),new System.Text.UTF8Encoding(false));
                return state.windows64 && state.game_exists && state.bridge_exists ? 0 : 2;
            }
            if(!Environment.Is64BitOperatingSystem)throw new Exception("Windows 10/11 64비트 PC가 필요합니다.");
            if(!File.Exists(Game))throw new Exception("ZIP 전체를 압축 해제한 폴더에서 실행해 주세요. Windows 폴더와 실행 파일을 함께 보관해야 합니다.");
            Directory.CreateDirectory(Data);
            if(!RuntimeInstalled())
            {
                string installer=Path.Combine(Root,"Prerequisites","vc_redist.x64.exe");
                if(!File.Exists(installer))throw new Exception("필수 구성요소가 없습니다. 배포 ZIP을 다시 압축 해제해 주세요.");
                if(MessageBox.Show("Microsoft Visual C++ 실행 구성요소 설치가 필요합니다. 설치 창을 열까요?","Imperial Wings",MessageBoxButtons.OKCancel,MessageBoxIcon.Information)!=DialogResult.OK)return 1;
                using(var install=Process.Start(new ProcessStartInfo(installer){UseShellExecute=true}))install.WaitForExit();
                if(!RuntimeInstalled())throw new Exception("구성요소 설치를 마친 뒤 다시 실행해 주세요.");
            }
            string bridgeExe=Path.Combine(Root,"Guidance","ImperialGuidance.exe");
            string run=Path.Combine(Data,"Runtime",Guid.NewGuid().ToString("N"));Directory.CreateDirectory(run);
            log=Path.Combine(run,"launcher.log");
            string ready=Path.Combine(run,"guidance-ready.json");
            string bridgeUrl=null;
            if(File.Exists(bridgeExe) && Array.IndexOf(args,"--manual-only")<0)
            {
                try
                {
                    var start=new ProcessStartInfo(bridgeExe,"--port 0 --ready-file "+Quote(ready)+" --parent-pid "+Process.GetCurrentProcess().Id){UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden,WorkingDirectory=Root,RedirectStandardOutput=true,RedirectStandardError=true};
                    bridge=new Process();bridge.StartInfo=start;
                    object logLock=new object();DataReceivedEventHandler receive=(s,e)=>{if(e.Data!=null)lock(logLock){try{File.AppendAllText(log,e.Data+Environment.NewLine);}catch(IOException){}}};
                    bridge.OutputDataReceived+=receive;bridge.ErrorDataReceived+=receive;
                    bridge.Start();bridge.BeginOutputReadLine();bridge.BeginErrorReadLine();
                    var deadline=DateTime.UtcNow.AddSeconds(30);
                    while(!File.Exists(ready) && !bridge.HasExited && DateTime.UtcNow<deadline)Thread.Sleep(100);
                    if(File.Exists(ready))
                    {
                        var health=Json.Deserialize<Dictionary<string,object>>(File.ReadAllText(ready));
                        var uri=new Uri((string)health["url"]);
                        if(uri.Scheme=="http" && uri.Host=="127.0.0.1" && Convert.ToInt32(health["protocol_version"])==2 && (string)health["source"]=="swarm-drone-atc")bridgeUrl=uri.GetLeftPart(UriPartial.Authority);
                    }
                }
                catch(Exception e){File.AppendAllText(log,"Guidance: "+e.Message+Environment.NewLine);}
            }
            if(bridgeUrl==null && Array.IndexOf(args,"--manual-only")<0)
                MessageBox.Show("자율 훈련 연결을 시작하지 못했습니다. 수동 키보드·USB 조종은 사용할 수 있습니다.\n진단 폴더: "+run,"Imperial Wings",MessageBoxButtons.OK,MessageBoxIcon.Information);
            string options="-windowed -ResX=1600 -ResY=1000 -ForceRes -NoScreenMessages -SaveToUserDir -ImperialDataDir="+Quote(Data);
            if(engineUserDirectory!=null)options+=" -UserDir="+Quote(engineUserDirectory);
            // Isolated unused loopback port means an unrelated old bridge is never adopted.
            options+=" -ImperialBridgeUrl="+(bridgeUrl??"http://127.0.0.1:1");
            if(!File.Exists(Path.Combine(Data,"Config","VisualQuality.txt")))options+=" -ImperialVisualQuality=Balanced";
            if(Array.IndexOf(args,"--setup-radio")>=0)options+=" -ImperialSetupRadio";
            File.AppendAllText(log,"Game: "+Game+Environment.NewLine);
            using(var game=Process.Start(new ProcessStartInfo(Game,options){UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(Game)}))
            {
                game.WaitForExit();
                if(game.ExitCode!=0)throw new Exception("게임을 시작하지 못했습니다. 최신 그래픽 드라이버와 DirectX 12 / Shader Model 6 지원 여부를 확인해 주세요.\n진단 폴더: "+run);
            }
            return 0;
        }
        catch(Exception e)
        {
            if(log!=null)try{File.AppendAllText(log,e.ToString());}catch(IOException){}
            MessageBox.Show(e.Message,"Imperial Wings",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;
        }
        finally
        {
            if(bridge!=null){try{if(!bridge.HasExited)bridge.Kill();}catch(InvalidOperationException){}finally{bridge.Dispose();}}
        }
    }
}
