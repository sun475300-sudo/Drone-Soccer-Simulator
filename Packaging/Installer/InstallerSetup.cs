using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

internal static class InstallerSetup
{
    const string Product="ImperialWingsSimulator";
    const string ReceiptName=".imperial-wings-install.json";
    const string RemoveName="ImperialWingsUninstall.exe";
    static readonly JavaScriptSerializer Json=new JavaScriptSerializer { MaxJsonLength=16777216 };
    static readonly StringComparison Ignore=StringComparison.OrdinalIgnoreCase;
    static FileStream ResultOutput;
    static readonly string PersonalData=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),Product,"Saved");
    static readonly string Pointer=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"ImperialWingsInstaller","last-install.txt");
    sealed class Receipt {
        public string product {get;set;} public int schema {get;set;} public string version {get;set;}
        public string target {get;set;} public string install_id {get;set;}
        public Dictionary<string,string> files {get;set;}
    }
    sealed class Result {
        public bool ok {get;set;} public string action {get;set;} public string target {get;set;}
        public string version {get;set;} public string backup {get;set;} public string message {get;set;}
        public bool rollback_restored {get;set;} public bool personal_data_untouched {get;set;}
        public List<string> retained_files {get;set;}
    }
    static string Full(string p) { string full=Path.GetFullPath(p),root=Path.GetPathRoot(full),trimmed=full.TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar);return String.Equals(trimmed,root.TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar),Ignore)?root.TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar)+Path.DirectorySeparatorChar:trimmed; }
    static bool Same(string a,string b){return String.Equals(Full(a),Full(b),Ignore);}
    static bool Within(string path,string root){string p=Full(path),r=Full(root);return String.Equals(p,r,Ignore)||p.StartsWith(r.TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar)+Path.DirectorySeparatorChar,Ignore);}
    static string Hash(Stream s){using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(s)).Replace("-","").ToLowerInvariant();}
    static string HashFile(string p){using(var s=File.OpenRead(p))return Hash(s);}
    static bool Reparse(string p){return (File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0;}
    static void CheckAncestors(string path){
        for(string p=Full(path);!String.IsNullOrEmpty(p);p=Path.GetDirectoryName(p))
            if((Directory.Exists(p)||File.Exists(p))&&Reparse(p))throw new IOException("연결 폴더(정션/심볼릭 링크)에는 설치하거나 제거하지 않습니다.");
    }
    static string Target(string raw){
        if(String.IsNullOrWhiteSpace(raw)||raw.Length<3||!Char.IsLetter(raw[0])||raw[1]!=':'||(raw[2]!='\\'&&raw[2]!='/'))throw new IOException("로컬 드라이브의 전체 설치 경로를 선택해 주세요.");
        string p=Full(raw),drive=Full(Path.GetPathRoot(p));
        if(Same(p,drive)||p.Length<drive.Length+4||Within(p,PersonalData))throw new IOException("드라이브 루트나 개인 보정·기록 폴더는 대상이 될 수 없습니다.");
        foreach(string special in new[]{Environment.GetFolderPath(Environment.SpecialFolder.Windows),Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86)})
            if(!String.IsNullOrEmpty(special)&&Same(p,special))throw new IOException("시스템 또는 사용자 기본 폴더 자체는 설치 대상이 될 수 없습니다.");
        if(Within(p,Environment.GetFolderPath(Environment.SpecialFolder.Windows)))throw new IOException("Windows 시스템 폴더에는 설치하거나 제거하지 않습니다.");
        if(p.Length>170)throw new IOException("설치 경로가 너무 깁니다. 더 짧은 폴더를 선택해 주세요.");
        CheckAncestors(p);return p;
    }
    static string SafeFile(string root,string rel){
        if(String.IsNullOrWhiteSpace(rel)||Path.IsPathRooted(rel)||rel.Contains(":"))throw new IOException("잘못된 배포 파일 경로입니다.");
        string p=Full(Path.Combine(root,rel.Replace('/',Path.DirectorySeparatorChar)));
        if(Same(p,root)||!Within(p,root)||p.Length>245)throw new IOException("배포 파일 경로가 설치 폴더를 벗어나거나 너무 깁니다.");
        CheckAncestors(Path.GetDirectoryName(p));return p;
    }
    static void NoPrivate(string rel){
        foreach(string part in rel.Replace('\\','/').Split('/'))
            if(new[]{"Saved","RadioProfile.json","PilotControls.json","PilotResponse.json","Language.txt",".git",".venv"}.Any(x=>String.Equals(x,part,Ignore)))throw new IOException("개인 데이터 또는 개발 파일은 설치 배포물에 포함할 수 없습니다.");
    }
    static List<string> Files(string root){
        var found=new List<string>();var pending=new Stack<string>();pending.Push(root);
        while(pending.Count>0){string d=pending.Pop();if(Reparse(d))throw new IOException("설치 폴더 안의 연결 경로를 거부했습니다.");
            foreach(string p in Directory.GetFiles(d)){if(Reparse(p))throw new IOException("연결 파일을 거부했습니다.");found.Add(p);}
            foreach(string p in Directory.GetDirectories(d))pending.Push(p);
        }return found;
    }
    static string Rel(string p,string root){return Full(p).Substring(Full(root).Length+1).Replace('\\','/');}
    static Dictionary<string,string> Hashes(string root){
        string file=Path.Combine(root,"SHA256SUMS.txt");if(!File.Exists(file))throw new IOException("배포 파일 해시 목록이 없습니다.");
        var hashes=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        foreach(string line in File.ReadAllLines(file,Encoding.UTF8)){
            int split=line.IndexOf("  ",StringComparison.Ordinal);if(split!=64)throw new IOException("잘못된 해시 목록입니다.");
            string h=line.Substring(0,64),r=line.Substring(66);if(h.Any(c=>!Uri.IsHexDigit(c))||hashes.ContainsKey(r))throw new IOException("중복 또는 잘못된 배포 해시입니다.");
            NoPrivate(r);SafeFile(root,r);hashes.Add(r,h.ToLowerInvariant());
        }
        hashes.Add("SHA256SUMS.txt",HashFile(file));return hashes;
    }
    static string ReleaseVersion(string root){
        string path=Path.Combine(root,"release-manifest.json");if(!File.Exists(path))throw new IOException("Imperial Wings 배포 정보가 없습니다.");
        var m=Json.Deserialize<Dictionary<string,object>>(File.ReadAllText(path,Encoding.UTF8));object v,privateIncluded;
        if(!m.TryGetValue("version",out v)||String.IsNullOrWhiteSpace(Convert.ToString(v))||!m.TryGetValue("private_profiles_included",out privateIncluded)||!Object.Equals(privateIncluded,false))throw new IOException("지원하지 않는 배포 정보입니다.");
        foreach(string rel in new[]{"ImperialWingsLauncher.exe","Windows/ImperialWingsSimulator.exe","Guidance/ImperialGuidance.exe","Prerequisites/vc_redist.x64.exe"})
            if(!File.Exists(SafeFile(root,rel)))throw new IOException("필수 실행 파일이 없습니다: "+rel);
        return Convert.ToString(v);
    }
    static Receipt ReadReceipt(string root){
        string p=Path.Combine(root,ReceiptName);if(!File.Exists(p))return null;
        var r=Json.Deserialize<Receipt>(File.ReadAllText(p,Encoding.UTF8));
        if(r==null||r.product!=Product||r.schema!=1||!Same(r.target,root)||r.files==null||r.files.Count<5||String.IsNullOrEmpty(r.install_id))throw new IOException("설치 정보가 손상되었거나 다른 경로의 정보입니다.");
        foreach(var entry in r.files){NoPrivate(entry.Key);SafeFile(root,entry.Key);if(entry.Value==null||entry.Value.Length!=64||entry.Value.Any(c=>!Uri.IsHexDigit(c)))throw new IOException("설치 파일 목록이 잘못되었습니다.");}
        return r;
    }
    static Dictionary<string,string> Verify(string root,bool update){
        Target(root);string version=ReleaseVersion(root);var hashes=Hashes(root);Receipt receipt=ReadReceipt(root);
        foreach(var item in hashes){string p=SafeFile(root,item.Key);if(!File.Exists(p)||HashFile(p)!=item.Value)throw new IOException("배포 파일 해시가 일치하지 않습니다: "+item.Key);}
        var allowed=receipt==null?hashes:receipt.files;
        foreach(var entry in allowed){string p=SafeFile(root,entry.Key);if(!File.Exists(p)||HashFile(p)!=entry.Value)throw new IOException("설치 파일이 없거나 변경되었습니다: "+entry.Key);}
        if(receipt!=null&&receipt.version!=version)throw new IOException("설치 버전 정보가 일치하지 않습니다.");
        var unknown=Files(root).Select(p=>Rel(p,root)).Where(p=>!allowed.ContainsKey(p)&&!String.Equals(p,ReceiptName,Ignore)).ToList();
        if(update&&unknown.Count>0)throw new IOException("기존 폴더에 추가 파일 또는 개인 기록이 있습니다. 내용을 보존하려면 별도 폴더 설치를 선택해 주세요.");
        return allowed;
    }
    static void NotRunning(string root,Dictionary<string,string> owned){
        foreach(Process p in Process.GetProcesses())try{using(p){string f=p.MainModule.FileName;if(Within(f,root))throw new IOException("실행 중인 Imperial Wings를 먼저 정상 종료한 뒤 다시 시도해 주세요.");}}catch(System.ComponentModel.Win32Exception){}catch(InvalidOperationException){}
        foreach(string rel in owned.Keys){string p=SafeFile(root,rel);if(File.Exists(p))using(File.Open(p,FileMode.Open,FileAccess.Read,FileShare.None)){} }
    }
    static void DeleteCreatedStage(string stage,string parent){
        if(!Full(stage).StartsWith(Full(parent).TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar)+Path.DirectorySeparatorChar+".iw-stage-",Ignore))throw new IOException("임시 폴더 정리 경로 검증에 실패했습니다.");
        if(!Directory.Exists(stage))return;Files(stage);Directory.Delete(stage,true);
    }

    static long Extract(string stage){
        var assembly=Assembly.GetExecutingAssembly();
        using(var stream=assembly.GetManifestResourceStream("payload.zip")){
            if(stream==null)throw new IOException("설치 데이터가 없습니다.");
            string hash=Hash(stream);if(hash!=InstallerPayloadInfo.Sha256)throw new IOException("설치 파일이 손상되었습니다. 공식 릴리스에서 다시 받아 주세요.");stream.Position=0;
            using(var zip=new ZipArchive(stream,ZipArchiveMode.Read)){
                string prefix=null;long total=0;var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach(var entry in zip.Entries){
                    string name=entry.FullName.Replace('\\','/');if(name.StartsWith("/")||name.Contains(":")||name.Split('/').Any(x=>x==".."))throw new IOException("배포 압축 경로를 거부했습니다.");
                    int slash=name.IndexOf('/');if(slash<1)throw new IOException("배포 압축의 최상위 폴더가 잘못되었습니다.");
                    string top=name.Substring(0,slash+1);if(prefix==null)prefix=top;else if(prefix!=top)throw new IOException("배포 압축의 폴더 구성이 다릅니다.");
                    string relative=name.Substring(slash+1);if(relative.Length==0||name.EndsWith("/"))continue;
                    NoPrivate(relative);if(!names.Add(relative))throw new IOException("중복된 배포 파일을 거부했습니다.");
                    if(((entry.ExternalAttributes>>16)&0xf000)==0xa000)throw new IOException("배포의 심볼릭 링크를 거부했습니다.");
                    total+=entry.Length;if(total>4294967296L||entry.Length>2147483648L)throw new IOException("배포 압축의 크기 제한을 초과했습니다.");
                }
                if(prefix==null||names.Count<5)throw new IOException("비어 있거나 불완전한 설치 데이터입니다.");
                var drive=new DriveInfo(Path.GetPathRoot(stage));if(drive.AvailableFreeSpace<total+33554432L)throw new IOException("설치 공간이 부족합니다. 다른 드라이브를 선택해 주세요.");
                foreach(var entry in zip.Entries){string name=entry.FullName.Replace('\\','/');string rel=name.Substring(prefix.Length);if(rel.Length==0||name.EndsWith("/"))continue;
                    string target=SafeFile(stage,rel);Directory.CreateDirectory(Path.GetDirectoryName(target));
                    using(var source=entry.Open())using(var output=new FileStream(target,FileMode.CreateNew,FileAccess.Write,FileShare.None))source.CopyTo(output);
                }
                return total;
            }
        }
    }
    static void MakeReceipt(string stage,string target){
        using(var resource=Assembly.GetExecutingAssembly().GetManifestResourceStream("uninstall.exe")){
            if(resource==null)throw new IOException("제거 프로그램이 없습니다.");
            using(var output=new FileStream(Path.Combine(stage,RemoveName),FileMode.CreateNew,FileAccess.Write,FileShare.None))resource.CopyTo(output);
        }
        var receipt=new Receipt{product=Product,schema=1,version=ReleaseVersion(stage),target=target,install_id=Guid.NewGuid().ToString("N"),files=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase)};
        foreach(string file in Files(stage)){string rel=Rel(file,stage);SafeFile(target,rel);receipt.files.Add(rel,HashFile(file));}
        SafeFile(target,ReceiptName);
        File.WriteAllText(Path.Combine(stage,ReceiptName),Json.Serialize(receipt),new UTF8Encoding(false));
    }
    static void WorkOutside(string target){if(!Within(Environment.CurrentDirectory,target))return;string outside=Full(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location));if(Within(outside,target))throw new IOException("\uC124\uCE58 \uD504\uB85C\uADF8\uB7A8\uC744 \uC124\uCE58 \uB300\uC0C1 \uD3F4\uB354 \uBC16\uC5D0\uC11C \uC2E4\uD589\uD574 \uC8FC\uC138\uC694.");CheckAncestors(outside);Directory.SetCurrentDirectory(outside);}
    static Result Install(string raw,string mode,bool register,string fail){
        string target=Target(raw),parent=Path.GetDirectoryName(target);bool existing=Directory.Exists(target),moved=false,committed=false,emptyRemoved=false;
        if(mode!="fresh"&&mode!="update")throw new IOException("설치 방식을 명시적으로 선택해 주세요.");
        if(File.Exists(target))throw new IOException("선택한 경로는 폴더가 아닌 파일입니다.");
        if(mode=="fresh"&&existing&&Directory.EnumerateFileSystemEntries(target).Any())throw new IOException("선택한 폴더에 파일이 있습니다. 빈 새 폴더를 선택하거나 기존 버전 업데이트를 선택해 주세요.");
        Dictionary<string,string> old=null;
        if(mode=="update"){
            if(!existing)throw new IOException("업데이트할 기존 설치 폴더를 선택해 주세요.");
            old=Verify(target,true);NotRunning(target,old);
            string self=Assembly.GetExecutingAssembly().Location;if(Within(self,target))throw new IOException("설치 프로그램을 설치 폴더 밖으로 옮긴 뒤 업데이트해 주세요.");
        }
        WorkOutside(target);Directory.CreateDirectory(parent);CheckAncestors(parent);
        string stage=Path.Combine(parent,".iw-stage-"+Guid.NewGuid().ToString("N")),backup=null;
        Directory.CreateDirectory(stage);
        try{
            Extract(stage);string version=ReleaseVersion(stage);if(version!=InstallerPayloadInfo.Version)throw new IOException("내장 데이터와 설치 버전이 다릅니다.");
            Verify(stage,true);MakeReceipt(stage,target);
            if(mode=="update"){
                Verify(target,true);NotRunning(target,old);
                backup=target+".backup-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8);
                CheckAncestors(backup);foreach(string rel in old.Keys)SafeFile(backup,rel);SafeFile(backup,ReceiptName);if(Directory.Exists(backup)||File.Exists(backup))throw new IOException("백업 폴더가 이미 있습니다.");
                Directory.Move(target,backup);moved=true;
            }
            if(mode=="fresh"&&existing){Directory.Delete(target);emptyRemoved=true;}
            if(fail=="after-backup")throw new IOException("격리 시험: 폴더 교체 직전 실패를 재현했습니다.");
            Directory.Move(stage,target);committed=true;
            string warning="설치가 완료되었습니다.";
            if(register)try{Directory.CreateDirectory(Path.GetDirectoryName(Pointer));File.WriteAllText(Pointer,target,new UTF8Encoding(false));}catch(IOException){warning+=" 최근 설치 위치를 기록하지 못했습니다. 실행기는 선택한 설치 폴더에 있습니다.";}catch(UnauthorizedAccessException){warning+=" 최근 설치 위치를 기록하지 못했습니다. 선택한 폴더에서 실행해 주세요.";}
            return new Result{ok=true,action=mode,target=target,version=version,backup=backup,message=warning,personal_data_untouched=true,retained_files=new List<string>()};
        }catch(Exception e){
            bool restored=!moved;
            if(emptyRemoved&&!committed&&!Directory.Exists(target))try{Directory.CreateDirectory(target);}catch(Exception){}
            if(moved&&!committed&&!Directory.Exists(target))try{Directory.Move(backup,target);restored=true;}catch(Exception rollback){throw new IOException("업데이트를 완료하지 못했고 자동 복원도 실패했습니다. 이전 버전은 "+backup+" 에 보관되어 있습니다. "+rollback.Message,e);}
            try{DeleteCreatedStage(stage,parent);}catch(Exception){}
            throw new InstallException(e.Message,restored,backup,e);
        }
    }
    sealed class InstallException:IOException {
        public bool Restored;public string Backup;
        public InstallException(string message,bool restored,string backup,Exception inner):base(message,inner){Restored=restored;Backup=backup;}
    }
    static Result Remove(string raw,bool register){
        string target=Target(raw);if(!Directory.Exists(target))throw new IOException("제거할 설치 폴더가 없습니다.");
        Receipt receipt=ReadReceipt(target);if(receipt==null)throw new IOException("설치기가 관리하는 폴더가 아닙니다. 수동 압축본의 파일은 자동 삭제하지 않습니다.");
        Files(target);NotRunning(target,receipt.files);WorkOutside(target);
        var retained=new List<string>();
        foreach(var item in receipt.files){string p=SafeFile(target,item.Key);if(File.Exists(p)){if(HashFile(p)==item.Value)File.Delete(p);else retained.Add(item.Key);}}
        // Unknown and modified files are preserved; never recurse through a junction.
        File.Delete(Path.Combine(target,ReceiptName));
        var dirs=new List<string>();var stack=new Stack<string>();stack.Push(target);
        while(stack.Count>0){string d=stack.Pop();foreach(string child in Directory.GetDirectories(d)){if(Reparse(child))throw new IOException("제거 중 연결 폴더를 발견했습니다.");dirs.Add(child);stack.Push(child);}}
        foreach(string d in dirs.OrderByDescending(x=>x.Length))if(!Directory.EnumerateFileSystemEntries(d).Any())Directory.Delete(d);
        foreach(string f in Files(target))if(!retained.Contains(Rel(f,target)))retained.Add(Rel(f,target));
        if(!Directory.EnumerateFileSystemEntries(target).Any())Directory.Delete(target);
        if(register&&File.Exists(Pointer))try{if(Same(File.ReadAllText(Pointer,Encoding.UTF8),target))File.Delete(Pointer);}catch(IOException){}catch(UnauthorizedAccessException){}
        return new Result{ok=true,action="uninstall",target=target,version=receipt.version,message=retained.Count==0?"프로그램 파일을 제거했습니다. 개인 보정·설정·기록은 유지했습니다.":"프로그램을 제거하고 추가·변경 파일은 보존했습니다. 남은 폴더를 직접 확인해 주세요.",personal_data_untouched=true,retained_files=retained};
    }
    static void Launch(string raw){string target=Target(raw);Verify(target,false);string file=SafeFile(target,"ImperialWingsLauncher.exe");Process.Start(new ProcessStartInfo(file){UseShellExecute=true,WorkingDirectory=target});}
    static string Quote(string value){var b=new StringBuilder("\"");int n=0;foreach(char c in value){if(c=='\\'){++n;continue;}if(c=='\"'){b.Append('\\',n*2+1);b.Append(c);}else{b.Append('\\',n);b.Append(c);}n=0;}b.Append('\\',n*2);return b.Append('\"').ToString();}

    static string KnownInstall(){try{if(File.Exists(Pointer)){string p=Target(File.ReadAllText(Pointer,Encoding.UTF8));if(Directory.Exists(p))return p;}}catch(Exception){}return null;}
    static void PrepareResult(string output,string target){
        if(String.IsNullOrEmpty(output))return;
        if(output.Length<3||!Char.IsLetter(output[0])||output[1]!=':'||(output[2]!='\\'&&output[2]!='/'))throw new IOException("결과 파일은 전체 로컬 경로로 지정해 주세요.");
        string p=Full(output);
        string destination=null;if(!String.IsNullOrEmpty(target))try{destination=Target(target);}catch(Exception){}
        if(Within(p,PersonalData)||Within(p,Environment.GetFolderPath(Environment.SpecialFolder.Windows))||(destination!=null&&Within(p,destination)))throw new IOException("결과 파일은 개인 데이터·설치 폴더 밖의 새 파일이어야 합니다.");
        CheckAncestors(p);Directory.CreateDirectory(Path.GetDirectoryName(p));
        ResultOutput=new FileStream(p,FileMode.CreateNew,FileAccess.Write,FileShare.None);
    }
    static void WriteResult(Result r,string output){if(ResultOutput!=null){byte[] bytes=new UTF8Encoding(false).GetBytes(Json.Serialize(r));ResultOutput.Position=0;ResultOutput.SetLength(0);ResultOutput.Write(bytes,0,bytes.Length);ResultOutput.Flush(true);}}
    static Result Failure(string action,string target,Exception e){var install=e as InstallException;return new Result{ok=false,action=action,target=target,message=e.Message,backup=install==null?null:install.Backup,rollback_restored=install!=null&&install.Restored,personal_data_untouched=true,retained_files=new List<string>()};}
    static string Get(Dictionary<string,string> o,string key,string fallback=null){string v;return o.TryGetValue(key,out v)?v:fallback;}
    static Dictionary<string,string> Options(string[] args){
        var o=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);var flags=new[]{"--quiet","--no-register"};
        var values=new[]{"--install","--uninstall","--verify","--mode","--result","--test-fail","--test-root","--wait-pid"};
        for(int i=0;i<args.Length;++i){string a=args[i];if(o.ContainsKey(a))throw new IOException("중복된 옵션입니다.");if(flags.Contains(a))o.Add(a,"true");else if(values.Contains(a)&&i+1<args.Length)o.Add(a,args[++i]);else throw new IOException("알 수 없거나 값이 없는 옵션입니다: "+a);}
        return o;
    }
    static void SelfRemove(string target){
        string self=Assembly.GetExecutingAssembly().Location;
        if(!Within(self,target))throw new IOException("제거 프로그램의 위치가 설치 폴더와 다릅니다.");
        string helperDir=Path.Combine(Path.GetTempPath(),"ImperialWingsRemove-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(helperDir);
        string helper=Path.Combine(helperDir,"ImperialWingsUninstall.exe");File.Copy(self,helper,false);
        var info=new ProcessStartInfo(helper,"--uninstall "+Quote(target)+" --wait-pid "+Process.GetCurrentProcess().Id){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=helperDir};Process.Start(info);
    }
    [STAThread] static int Main(string[] args){
        bool quiet=Array.IndexOf(args,"--quiet")>=0;string output=null,action="setup",target=null;
        try{
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
#if UNINSTALL_ONLY
            if(args.Length==0){
                target=Target(AppDomain.CurrentDomain.BaseDirectory);
                if(ReadReceipt(target)==null)throw new IOException("정상 설치 정보가 없습니다. 자동 제거하지 않습니다.");
                if(MessageBox.Show("이 설치 폴더의 프로그램 파일을 제거할까요?\n개인 조종기 보정·설정·기록은 유지됩니다.\n"+target,"Imperial Wings 제거",MessageBoxButtons.OKCancel,MessageBoxIcon.Question)!=DialogResult.OK)return 1;
                SelfRemove(target);return 0;
            }
#endif
            if(args.Length==0){Application.Run(new SetupForm());return 0;}
            var o=Options(args);output=Get(o,"--result");
            int commands=new[]{"--install","--uninstall","--verify"}.Count(k=>o.ContainsKey(k));if(commands!=1)throw new IOException("설치·제거·검사 중 한 가지 작업을 지정해 주세요.");
            action=o.ContainsKey("--install")?"install":o.ContainsKey("--uninstall")?"uninstall":"verify";target=Get(o,"--"+action);
            PrepareResult(output,target);
            string failure=Get(o,"--test-fail");
            if(failure!=null){string test=Get(o,"--test-root");if(failure!="after-backup"||test==null||!Within(Target(target),Target(test))||Same(target,test))throw new IOException("오류 주입은 명시한 격리 시험 폴더 아래에서만 가능합니다.");}
            string wait=Get(o,"--wait-pid");if(wait!=null){
                int id;if(!Int32.TryParse(wait,out id)||id<=0)throw new IOException("잘못된 제거 대기 프로세스입니다.");
                try{using(var parent=Process.GetProcessById(id)){try{if(!parent.HasExited){
                    if(!Within(parent.MainModule.FileName,Target(target)))throw new IOException("제거 대기 프로세스의 위치가 다릅니다.");
                    if(!parent.WaitForExit(30000))throw new IOException("제거 프로그램이 종료되지 않았습니다. 다시 시도해 주세요.");
                }}catch(InvalidOperationException){if(!parent.HasExited)throw;}catch(System.ComponentModel.Win32Exception){if(!parent.HasExited)throw;}}}catch(ArgumentException){}
            }
            Result result;
            if(action=="install")result=Install(target,Get(o,"--mode"),!o.ContainsKey("--no-register"),failure);
            else if(action=="uninstall")result=Remove(target,!o.ContainsKey("--no-register"));
            else{target=Target(target);Verify(target,false);result=new Result{ok=true,action="verify",target=target,version=ReleaseVersion(target),message="설치 파일 검사를 통과했습니다.",personal_data_untouched=true};}
            WriteResult(result,output);if(!quiet)MessageBox.Show(result.message,"Imperial Wings",MessageBoxButtons.OK,MessageBoxIcon.Information);return 0;
        }catch(Exception e){
            try{WriteResult(Failure(action,target,e),output);}catch(Exception){}
            if(!quiet)MessageBox.Show(e.Message,"Imperial Wings",MessageBoxButtons.OK,MessageBoxIcon.Error);return 2;
        }finally{if(ResultOutput!=null)ResultOutput.Dispose();}
    }
    sealed class SetupForm:Form {
        readonly TextBox folder=new TextBox();readonly Label status=new Label();readonly RadioButton fresh=new RadioButton(),update=new RadioButton();
        readonly Button install=new Button(),browse=new Button(),remove=new Button(),existing=new Button();readonly CheckBox launch=new CheckBox();bool working;
        public SetupForm(){
            Text="Imperial Wings "+InstallerPayloadInfo.Version+" 설치";ClientSize=new System.Drawing.Size(640,455);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;StartPosition=FormStartPosition.CenterScreen;
            var title=new Label{Text="Imperial Wings 설치 위치와 방식을 선택하세요",Left=22,Top=20,Width=596,Height=28,Font=new System.Drawing.Font("맑은 고딕",12,System.Drawing.FontStyle.Bold)};
            var info=new Label{Text="전체 게임과 실행기가 함께 설치됩니다. 새 도구나 관리자 권한을 자동 요청하지 않습니다.\n별도 폴더에 설치해도 이 Windows 사용자의 보정·설정·기록은 버전 간 공유됩니다.",Left=22,Top=56,Width=596,Height=48};
            fresh.Text="새 폴더에 별도 설치";fresh.SetBounds(22,112,270,24);fresh.Checked=true;
            update.Text="선택한 기존 버전 업데이트";update.SetBounds(310,112,300,24);
            folder.SetBounds(22,151,500,27);folder.Text=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs","ImperialWings","v"+InstallerPayloadInfo.Version);
            browse.Text="폴더 선택";browse.SetBounds(534,150,84,28);browse.Click+=(s,e)=>{using(var d=new FolderBrowserDialog{Description="별도 설치할 새 폴더 이름을 지정하거나 기존 버전 폴더를 선택하세요.",SelectedPath=Directory.Exists(folder.Text)?folder.Text:Path.GetDirectoryName(folder.Text),ShowNewFolderButton=true})if(d.ShowDialog(this)==DialogResult.OK)folder.Text=d.SelectedPath;};
            existing.Text="이전에 설치한 위치 선택";existing.SetBounds(22,189,230,28);string known=KnownInstall();existing.Enabled=known!=null;existing.Click+=(s,e)=>{string selected=KnownInstall();if(selected!=null){folder.Text=selected;update.Checked=true;}};
            status.SetBounds(22,229,596,77);status.Text="새 폴더 설치는 기존 버전을 교체하지 않습니다.";
            var policy=new Label{Text="업데이트 전 기존 프로그램을 닫아 주세요. 이전 버전은 백업 폴더에 보관됩니다.\n추가 파일·개인 Saved·인식되지 않는 폴더는 덮어쓰지 않습니다.\n이 배포물은 서명되지 않아 Windows 게시자·SmartScreen 경고가 나타날 수 있습니다.",Left=22,Top=310,Width=596,Height=60};
            launch.Text="완료 후 게임 실행";launch.SetBounds(22,384,205,24);launch.Checked=false;
            remove.Text="이 설치 제거";remove.SetBounds(286,383,120,32);remove.Click+=(s,e)=>RunTask(true);
            install.Text="선택한 방식으로 설치";install.SetBounds(420,383,198,32);install.Click+=(s,e)=>RunTask(false);
            folder.TextChanged+=(s,e)=>RefreshStatus();fresh.CheckedChanged+=(s,e)=>RefreshStatus();update.CheckedChanged+=(s,e)=>RefreshStatus();
            Controls.AddRange(new Control[]{title,info,fresh,update,folder,browse,existing,status,policy,launch,remove,install});
            FormClosing+=(s,e)=>{if(working){e.Cancel=true;MessageBox.Show("설치 또는 제거가 진행 중입니다. 완료된 뒤 창을 닫아 주세요.","작업 진행 중");}};RefreshStatus();
        }
        void RefreshStatus(){
            if(working)return;remove.Enabled=false;
            try{string path=Target(folder.Text);if(Directory.Exists(path)){string v=ReleaseVersion(path);remove.Enabled=ReadReceipt(path)!=null;status.Text="선택한 폴더의 버전: "+v+"\n"+(update.Checked?"전체 파일 확인 후 업데이트합니다. 추가·변경 파일이 있으면 중단합니다.":"폴더가 이미 있습니다. 업데이트를 선택하거나 다른 새 폴더를 지정해 주세요.");}else status.Text=update.Checked?"업데이트할 기존 설치 폴더를 선택해 주세요.":"새 폴더에 "+InstallerPayloadInfo.Version+"을 설치합니다.";}
            catch(Exception e){status.Text=e.Message;}
        }
        void RunTask(bool uninstall){
            if(working)return;string path=folder.Text,mode=update.Checked?"update":"fresh";
            if(uninstall&&MessageBox.Show("이 설치 폴더의 프로그램만 제거할까요?\n개인 보정·설정·기록과 추가 파일은 보존합니다.","제거 확인",MessageBoxButtons.OKCancel,MessageBoxIcon.Question)!=DialogResult.OK)return;
            if(!uninstall&&mode=="update"&&MessageBox.Show("선택한 폴더의 프로그램을 새 버전으로 교체할까요?\n개인 보정·설정·기록은 유지하고 이전 프로그램은 백업합니다.\n"+path,"업데이트 확인",MessageBoxButtons.OKCancel,MessageBoxIcon.Question)!=DialogResult.OK)return;
            bool start=launch.Checked;working=true;install.Enabled=browse.Enabled=remove.Enabled=existing.Enabled=folder.Enabled=fresh.Enabled=update.Enabled=launch.Enabled=false;UseWaitCursor=true;status.Text="파일을 확인하고 있습니다. 완료될 때까지 기다려 주세요.";
            var worker=new System.ComponentModel.BackgroundWorker();worker.DoWork+=(s,e)=>{e.Result=uninstall?Remove(path,true):Install(path,mode,true,null);};
            worker.RunWorkerCompleted+=(s,e)=>{working=false;UseWaitCursor=false;existing.Enabled=KnownInstall()!=null;install.Enabled=browse.Enabled=folder.Enabled=fresh.Enabled=update.Enabled=launch.Enabled=true;
                if(e.Error!=null){status.Text=e.Error.Message;MessageBox.Show(e.Error.Message,"작업을 완료하지 못했습니다",MessageBoxButtons.OK,MessageBoxIcon.Error);}else{var r=(Result)e.Result;status.Text=r.message+(r.backup==null?"":"\n이전 버전 백업: "+r.backup);MessageBox.Show(status.Text,"Imperial Wings",MessageBoxButtons.OK,MessageBoxIcon.Information);if(start&&!uninstall)try{Launch(r.target);}catch(Exception ex){MessageBox.Show("설치는 완료했지만 실행하지 못했습니다. 설치 폴더의 ImperialWingsLauncher.exe를 직접 열어 주세요.\n"+ex.Message,"실행 안내");}}
                RefreshStatus();};worker.RunWorkerAsync();
        }
    }
}
