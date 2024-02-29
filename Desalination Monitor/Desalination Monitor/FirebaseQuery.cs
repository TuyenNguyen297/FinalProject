using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Firebase.Database;
using Firebase.Database.Query;
using Xamarin.Essentials;
using System.Diagnostics;

namespace Desalination
{

    public class FirebaseQuery
    {
        FirebaseClient firebase;
        public FirebaseQuery()
        {
            firebase = new FirebaseClient("https://desalination-7bd70.firebaseio.com/");
        }

        public async Task<bool> UpdateCMD(string id, string CMD)
        {
            try
            {
                if (Connectivity.NetworkAccess == NetworkAccess.Internet)
                {
                    var toCMD = (await firebase
                .Child("Index")
                .OnceAsync<CMDCreation>()).Where(a => a.Object.ID == id).Single();

                    await firebase
                    .Child("Index")
                    .Child(toCMD.Key)
                    .PutAsync(new CMDCreation() { ID = id, CMD = CMD });
                    Trace.WriteLine("success");
                    return true;
                }
                else return false;
            }
            catch (Exception ex)
            {
                Trace.WriteLine("Fail:" + ex);
                return false;
                //throw new Exception("OnAppearing  Additional information..." + ex, ex);
            }
        }

        public async Task<Index> RetrieveIndex(string id)
        {
            try
            {
                if (Connectivity.NetworkAccess == NetworkAccess.Internet)
                {
                    var existIndex = await GetIndex();
                    await firebase
                        .Child("Index")
                        .OnceAsync<Index>();
                    Trace.WriteLine("Retrieved Index Succesfully!");
                    return existIndex.Where(a => a.ID == id).Single();
                }
                else return null;
            }
            catch (Exception ex)
            {
                Trace.WriteLine("Error Retrieving!");
                return null;

                //throw new Exception("OnAppearing  Additional information..." + ex, ex);
            }
        }

        public async Task<CMDCreation> RetrieveCMD(string id)
        {
            try
            {
                if (Connectivity.NetworkAccess == NetworkAccess.Internet)
                {
                    var existCMD = await GetCMD();
                    await firebase
                        .Child("Index")
                        .OnceAsync<CMDCreation>();
                    Trace.WriteLine("Retrieved CMD Succesfully!");
                    return existCMD.Where(a => a.ID == id).Single();
                }
                else return null;
            }
            catch (Exception ex)
            {
                Trace.WriteLine("Error Retrieving!");
                return null;

                //throw new Exception("OnAppearing  Additional information..." + ex, ex);
            }
        }

        //public async Task AddNewKey(string ID, string ROIndex)
        //{
        //    try
        //    {
        //        await firebase
        //          .Child("Index")
        //          .PostAsync(new Index() { ID = ID, ROIndex = ROIndex });
        //    }
        //    catch (Exception ex)
        //    {
        //        System.Diagnostics.Debug.WriteLine("Error:" + ex);
        //    }
        //}

        public async Task<List<Index>> GetIndex()
        {
            try
            {
                if (Connectivity.NetworkAccess == NetworkAccess.Internet)
                {
                    return (await firebase
                   .Child("Index")
                   .OnceAsync<Index>()).Select(item => new Index
                   {
                       ID = item.Object.ID,
                       ROIndex = item.Object.ROIndex,
                   }).ToList();
                }
                return null;
            }
            catch (Exception x)
            {
                Trace.WriteLine(x);
                return null;
            }
        }

        public async Task<List<CMDCreation>> GetCMD()
        {
            try
            {
                if (Connectivity.NetworkAccess == NetworkAccess.Internet)
                {
                    return (await firebase
                   .Child("Index")
                   .OnceAsync<CMDCreation>()).Select(item => new CMDCreation
                   {
                       ID = item.Object.ID,
                       CMD = item.Object.CMD
                   }).ToList();
                }
                return null;
            }
            catch (Exception x)
            {
                Trace.WriteLine(x);
                return null;
            }
        }
    }
}
