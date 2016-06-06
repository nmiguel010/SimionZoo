﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Caliburn.Micro;
using System.Windows.Threading;
using System.Dynamic;
using System.Windows;
using System.Xml;

namespace AppXML.ViewModels
{
    public class ButtonToWindowedClassViewModel :ValidableAndNodeViewModel
    {
        private XmlDocument _doc;
        private string m_validationColor= "Transparent";
        public string validationColor { get { return m_validationColor; } set { m_validationColor = value; } }
        
        private WindowClassViewModel m_windowedClassVM;
        public WindowClassViewModel windowedClassVM { get { return m_windowedClassVM; } set { m_windowedClassVM = value; } }

        private string _tag;
        public string Tag { get { return _tag; } set { _tag = value; } }

        private string m_className;

        private string m_name = "";
        public string name { get { return m_name; } set { m_name = value; } }
        public WindowClassViewModel Class { get { return m_windowedClassVM; } set { m_windowedClassVM = value; } }

        public ButtonToWindowedClassViewModel(XmlNode xmlNode,XmlDocument doc)
        {
            m_name = xmlNode.Attributes["Name"].Value;
            m_className= xmlNode.Attributes["Class"].Value;

            m_validationColor = "Transparent";

            m_windowedClassVM= new WindowClassViewModel(m_className,m_name,this,doc);
            validate();

      
            _tag = xmlNode.Attributes["Name"].Value;

            _doc = doc;
        }
        public void showWindow()
        {


            WindowManager windowManager = new WindowManager();
            dynamic settings = new ExpandoObject();
            settings.WindowStyle = WindowStyle.ThreeDBorderWindow;
            settings.ShowInTaskbar = true;
            settings.Title = m_name;
            settings.WindowState = WindowState.Normal;
            settings.ResizeMode = ResizeMode.CanMinimize;


            windowManager.ShowDialog(m_windowedClassVM, null, settings);



        }

        public override bool validate()
        {

            bool ok= m_windowedClassVM.validate();
            if (ok)
                m_validationColor = "Gray";
            else
                m_validationColor = "Red";
            NotifyOfPropertyChange(() => validationColor);

            return ok;
        }
        public override List<XmlNode> getXmlNode()
        {
            XmlNode result = AppXML.Data.Utility.resolveTag(_tag, _doc);
            XmlNode lastChild = AppXML.Data.Utility.getLastChild(result);
            foreach (XmlNode child in m_windowedClassVM.Class.getXmlNodes())
            {
                if (child != null)
                    lastChild.AppendChild(child);
            }

            List<XmlNode> list = new List<XmlNode>();
            list.Add(result);
            return list;
        }
    }
}