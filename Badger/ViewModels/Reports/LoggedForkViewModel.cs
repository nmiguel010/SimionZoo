﻿using System.Xml;
using System.Collections.Generic;
using Badger.Simion;
using System;

namespace Badger.ViewModels
{
    public class LoggedForkViewModel: SelectableTreeItem
    {
        private string m_name = "unnamed";
        public string name
        {
            get { return m_name; }
            set { m_name = value; }
        }
        private List<LoggedForkValueViewModel> m_values = new List<LoggedForkValueViewModel>();
        public List<LoggedForkValueViewModel> values
        {
            get { return m_values; }
            set { m_values = value; }
        }

        private List<LoggedForkViewModel> m_forks = new List<LoggedForkViewModel>();
        public List<LoggedForkViewModel> forks
        {
            get { return m_forks; }
            set { m_forks = value; }
        }


        public LoggedForkViewModel(XmlNode configNode, ReportsWindowViewModel parent)
        {
            m_parentWindow = parent;
            if (configNode.Attributes.GetNamedItem(XMLConfig.aliasAttribute) != null)
                name = configNode.Attributes[XMLConfig.aliasAttribute].Value;

            foreach(XmlNode child in configNode.ChildNodes)
            {
                if (child.Name==XMLConfig.forkTag)
                {
                    LoggedForkViewModel newFork = new LoggedForkViewModel(child, parent);
                    forks.Add(newFork);
                }
                else if (child.Name==XMLConfig.forkValueTag)
                {
                    LoggedForkValueViewModel newValue = new LoggedForkValueViewModel(child, parent);
                    values.Add(newValue);
                }
            }

            if (forks.Count == 0) bVisible = false;
        }

        public void groupByThisFork()
        {
            //this method is called from the context menu
            //informs the parent window that results should be grouped by this fork
            m_parentWindow.addGroupBy(name);
        }



        public override void TraverseAction(bool doActionLocally, Action<SelectableTreeItem> action)
        {
            if (doActionLocally) LocalTraverseAction(action);
            foreach (LoggedForkValueViewModel value in m_values) value.TraverseAction(true,action);
            foreach (LoggedForkViewModel fork in m_forks) fork.TraverseAction(true,action);
        }
    }
}