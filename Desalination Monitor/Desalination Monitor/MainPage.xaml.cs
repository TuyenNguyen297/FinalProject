using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Linq.Expressions;
using System.Threading.Tasks;
using System.Timers;
using Xamarin.Essentials;
using Xamarin.Forms;


namespace Desalination
{
    [DesignTimeVisible(false)]
    public partial class MainPage : ContentPage
    {
        private static Timer ESPTimerCheck;
        static FirebaseQuery Query;

        const int vibrateTime = 100;
        string EspStt;
        static int prevRandom = 0;
        bool initRandom;
        int curRandom = 0;

        string curState;
        string curCMD;

        string btnCMD;
        bool btnPressed;

        string[] allIndex;


        public MainPage()
        {
            InitializeComponent();
            Query = new FirebaseQuery();
            SetTimerSTT();
            //SetTimerIndex();
            GetIndex();
        }

        private void SetTimerSTT()
        {
            ESPTimerCheck = new Timer(4000);
            ESPTimerCheck.Elapsed += OnTimedEvent1;
            ESPTimerCheck.AutoReset = true;
            ESPTimerCheck.Enabled = true;
        }

        private void OnTimedEvent1(Object source, ElapsedEventArgs e)
        {
            GetEspStt();
        }

        private async void GetEspStt()
        {

            if (Connectivity.NetworkAccess == NetworkAccess.Internet)
            {
                Device.BeginInvokeOnMainThread(() =>
                {
                    wifiImg.Source = "ON.png";
                });
                if (!initRandom)
                {
                    try
                    {
                        var x = await Query.RetrieveIndex("IndexArea");
                        string[] index = StringSplit(x.ROIndex);
                        curRandom = Convert.ToInt32(index[4]);
                        //Trace.WriteLine("Init OK!");
                    }
                    catch
                    {
                        curRandom = 0;
                    }
                    prevRandom = curRandom;
                    initRandom = true;
                }
                //Trace.WriteLine("prev:" + prevRandom);
                //Trace.WriteLine("cur :" + curRandom);
                if (prevRandom != curRandom && curRandom != 0)
                {
                    prevRandom = curRandom;
                    Trace.Write("EspStt:" + EspStt);
                    Device.BeginInvokeOnMainThread(() =>
                    {
                        EspStt = "online";
                        espImg.Source = "ON.png";
                    });
                }
                else
                {
                    Device.BeginInvokeOnMainThread(() =>
                    {
                        EspStt = "offline";
                        espImg.Source = "OFF.png";
                    });
                }
            }
            else
            {
                Device.BeginInvokeOnMainThread(() =>
                {
                    EspStt = "offline";
                    wifiImg.Source = "OFF.png";
                    espImg.Source = "OFF.png";
                });
            }

        }

        //protected override void OnAppearing()
        //{
        //    base.OnAppearing();
        //    Connectivity.ConnectivityChanged += Connectivity_ConnectivityChanged;
        //}

        //public void Connectivity_ConnectivityChanged(object sender, ConnectivityChangedEventArgs e)
        //{
        //    var access = e.NetworkAccess;

        //    if (access != NetworkAccess.Internet)
        //    {
        //        //initRandom = false;
        //    }
        //}

        private async void GetIndex()
        {
            await Task.Run(() => Query.RetrieveCMD("CMDArea")).ContinueWith(cmd => ShowButton(cmd));
            await Task.Run(() => Query.RetrieveIndex("IndexArea")).ContinueWith(index => ShowIndex(index)).ContinueWith(task => GetIndex());
        }

        private async void ShowIndex(Task<Index> index)
        {
            try
            {
                allIndex = StringSplit(index.Result.ROIndex);
            }
            catch (Exception x)
            {
                allIndex = StringSplit(null);
            }

            string estt = allIndex[0];
            string stt = allIndex[0] == "RUN" ? "LỌC" : (allIndex[0] == "STOP" ? "DỪNG" : (allIndex[0] == "FULL" ? "ĐẦY" : null));
            string tds = allIndex[1];
            string vol = allIndex[2] == "0.0" ? "0" : allIndex[2];
            string rdm = allIndex[3];
            string sal = CvSalinity(tds);
            string qlt = CvQuality(tds);

            curRandom = rdm == null ? 0 : Convert.ToInt32(rdm);
            curState = estt;

            await Task.Run(() => Device.BeginInvokeOnMainThread(() =>
            {
                Status.Text = stt;
                TDS.Text = tds;
                Volume.Text = vol;
                Salinity.Text = sal;
                Quality.Text = qlt;
            }));
        }

        private void ShowButton(Task<CMDCreation> cmd)
        {
            try
            {
                curCMD = cmd.Result.CMD;
            }
            catch (Exception x)
            {
                curCMD = null;
            }
            if (btnPressed && curCMD != btnCMD)
            {
                btnPressed = false;
                curCMD = btnCMD;
            }
            if (curCMD == "RUN")
            {
                Device.BeginInvokeOnMainThread(() =>
                {
                    CMDbtn.TextColor = Color.Accent;
                    CMDbtn.Text = "DỪNG";
                });
            }
            else if (curCMD == "STOP")
            {
                Device.BeginInvokeOnMainThread(() =>
                {
                    CMDbtn.TextColor = Color.CornflowerBlue;
                    CMDbtn.Text = "LỌC";
                });
            }
            else
            {
                Device.BeginInvokeOnMainThread(() =>
                {
                    CMDbtn.TextColor = Color.Black;
                    CMDbtn.Text = "LỌC/DỪNG";
                });
            }

        }

        private string[] StringSplit(string index)
        {
            if (!string.IsNullOrEmpty(index))
            {
                String pattern = @",";
                String[] elements = System.Text.RegularExpressions.Regex.Split(index, pattern);
                foreach (var element in elements.ToList())
                    Console.WriteLine(element);
                return elements;
            }
            else return new string[] { null, null, null, null };
        }

        private string CvSalinity(string tds)
        {
            if (!string.IsNullOrEmpty(tds))
            {
                string Comparator = null;
                tds = tds.Replace(" ", string.Empty);
                if (!Char.IsNumber(tds[0]))
                {
                    Comparator = tds[0].ToString() + " ";
                    tds = tds.Substring(1);
                }
                double EC = Double.Parse(tds) / 0.64; // mS/cm nước sông suối, còn nước chứa nhiều tds thì 0.54
                double Salinity = EC / 2.12 / 10000; // %
                return (Comparator + (Salinity != 0 ? string.Format("{0:0.0000}", Salinity) : "0"));
            }
            else return null;
        }

        private string CvQuality(string tds)
        {
            if (!string.IsNullOrEmpty(tds))
            {
                //string quality = ((Convert.ToInt32(tds) >= 100) && (Convert.ToInt32(tds) <= 500)) ? "TỐT" : "KÉM";
                tds = tds.Replace(" ", string.Empty);
                tds = tds.Replace("<", string.Empty);
                string quality = (Convert.ToInt32(tds) <= 50) ? "TỐT" : "KÉM";
                return quality;
            }
            else return null;
        }

        private async Task<bool> SetCMD(string id, string CMD)
        {
            var updateFlag = await Query.UpdateCMD(id, CMD);
            return updateFlag;
        }

        private async void btn_Click(object sender, EventArgs e)
        {
            Vibration.Vibrate(vibrateTime);
            if (EspStt == "online")
            {
                string tempCMD = curState == "FULL" ? "STOP" : (curCMD == "RUN" ? "STOP" : "RUN");
                await Task.Run(() => SetCMD("CMDArea", tempCMD)).ContinueWith((isSuccessful) =>
                {
                    if (isSuccessful.Result)
                    {
                        btnPressed = true;
                        if (tempCMD == "RUN")
                        {
                            btnCMD = "RUN";
                            Device.BeginInvokeOnMainThread(() =>
                            {
                                CMDbtn.Text = "DỪNG";
                                CMDbtn.TextColor = Color.Accent;
                            });
                        }
                        else
                        {
                            btnCMD = "STOP";
                            Device.BeginInvokeOnMainThread(() =>
                            {
                                CMDbtn.Text = "LỌC";
                                CMDbtn.TextColor = Color.CornflowerBlue;
                            });
                        }
                    }
                });
            }
        }
        //private async void Btn1_Click(object sender, EventArgs e)
        //{
        //    try
        //    {
        //        await Task.Run(() => Query.AddNewKey("IndexArea", "STOP,0,0,0"));
        //    }
        //    catch  { }
        //}

        //private async void Btn2_Click(object sender, EventArgs e)
        //{
        //    try
        //    {
        //        await Task.Run(() => Query.AddNewKey("CMDArea", "STOP"));
        //    }
        //    catch { }
        //}
    }
}
