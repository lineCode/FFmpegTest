using System;
using System.IO;
using System.Runtime.InteropServices;

namespace ConsoleApplication1
{
    class Program
    {
        /// <summary>
        /// Обратный вызов получения фрейма
        /// </summary>
        protected static RtspWriteToFile.GetImageCallback _getImageCallback;

        static void Main(string[] args)
        {
            try
            {
                var rtsp = "rtsp://admin:admin@192.168.11.132:554/cam/realmonitor?channel=1&subtype=1";
               // var rtsp = "rtsp://admin:admin@192.168.11.185:554/cam/realmonitor?channel=1&subtype=1";
                var file = "D:\\test_file_ouput.mp4";
                
                //RtspWriteToFile.Record(rtsp, file);

                Streaming(rtsp);
            }
            catch (Exception e)
            {
                Console.WriteLine(e.Message);
            }

            Console.WriteLine("Run...");
            Console.ReadLine();
        }

        private static void Streaming(string rtsp)
        {
            var stream = File.OpenWrite("D:\\test.mp4");

            _getImageCallback = (bufferPtr, size) =>
            {
                var buffer = new byte[size];
                Marshal.Copy(bufferPtr, buffer, 0, size);

                stream.WriteAsync(buffer, 0, size);
            };

            RtspWriteToFile.Online(rtsp, _getImageCallback);
        }
    }
}
