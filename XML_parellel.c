#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <crtdbg.h>
#include <pthread.h>

#define MAX_THREAD 10
pthread_t thread[MAX_THREAD]; 
int thread_args[MAX_THREAD];

typedef struct{
	int start;
	char * str;
	int end;
	int isoutput; 
}Automata;

#define MAX_SIZE 50
Automata stateMachine[MAX_SIZE];

typedef struct Node{
int state;
struct Node ** children;
struct Node * start_node;
struct Node * finish_node;
char * output;
int hasOutput;
struct Node * parent;
int isLeaf;
}Node;

Node* start_root[MAX_THREAD];   //start tree
Node* finish_root[MAX_THREAD];   //finish tree

char* currentStr[MAX_THREAD];

#define MAX_LINE 100
static char multiExpContent[MAX_THREAD][MAX_LINE];
static char multiCDATAContent[MAX_THREAD][MAX_LINE];


int stateCount=0;
int machineCount=1;


typedef struct
{
char *p;
int len;
}
xml_Text;

typedef enum {
xml_tt_U, /* Unknow */
xml_tt_H, /* XML Head <?xxx?>*/
xml_tt_E, /* End Tag </xxx> */
xml_tt_B, /* Start Tag <xxx> */
xml_tt_BE, /* Tag <xxx/> */
xml_tt_T, /* Content for the tag <aaa>xxx</aaa> */
xml_tt_C, /* Comment <!--xx-->*/
xml_tt_ATN, /* Attribute Name <xxx id="">*/
xml_tt_ATV, /* Attribute Value <xxx id="222">*/
xml_tt_CDATA/* <![CDATA[xxxxx]]>*/
}
xml_TokenType;

typedef struct
{
xml_Text text;
xml_TokenType type;
}
xml_Token;

typedef struct QueueEle{
	Node* node;
	int layer;
}QueueEle;


typedef struct ResultSet
{
	int begin;
	int begin_stack[MAX_SIZE];
	int topbegin;
	int end;
	int end_stack[MAX_SIZE];
	int topend;
	char* output;
	int hasOutput;
}ResultSet;

#define MAX_ATT_NUM 50
char tokenValue[MAX_ATT_NUM][MAX_ATT_NUM]={"UNKNOWN","HEAD","NODE_END","NODE_BEGIN","NODE_BEGIN_END","TEXT","COMMENT","ATTRIBUTE_NAME","ATTRIBUTE_VALUE","CDATA"};
char defaultToken[MAX_ATT_NUM]="WRONG_INFO";
 
void createAutoMachine(char* xmlPath); 
int split_file(char* file_name, int one_size);
void createTree_first(int start_state);
void createTree(int thread_num);
void print_tree(Node* tree,int layer);
ResultSet getresult(int n);
void print_result(ResultSet set);
void add_node(Node* node, Node* root);  //for finish tree
void push(Node* node, Node* root, int nextState); //push new element into stack
int checkChildren(Node* node);  //return value--the number of children -1--no child
void pop(char * str, Node* root); //pop element due to end_tag e.g</d>
char* xml_substring(xml_Text *pText, int begin, int end);
char* substring(char *pText, int begin, int end);
char* convertTokenTypeToStr(xml_TokenType type);
int xml_initText(xml_Text *pText, char *s);
int xml_initToken(xml_Token *pToken, xml_Text *pText);
int xml_print(xml_Text *pText, int begin, int end);  //output the content for every parsed element
int xml_println(xml_Text *pText);
char* ltrim(char *s);
int xml_process(xml_Text *pText, xml_Token *pToken, int multilineExp, int multilineCDATA, int thread_num);  //parse and deal with every element in an xmlText, return value:0--success -1--error 1--multiline explantion 2--multiline CDATA

void createAutoMachine(char* xmlPath)
{
	char seps[] = "/"; 
	char *token = strtok(xmlPath, seps); 
	while(token!= NULL) 
	{
		stateCount++;
		stateMachine[machineCount].start=stateCount;
		stateMachine[machineCount].str=(char*)malloc((strlen(token)+1)*sizeof(char));
		stateMachine[machineCount].str=strcpy(stateMachine[machineCount].str,token);
		stateMachine[machineCount].end=stateCount+1;
		stateMachine[machineCount].isoutput=0;
		machineCount++;
		if(stateCount>=1)
		{
			stateMachine[machineCount].start=stateCount+1;
			stateMachine[machineCount].str=(char*)malloc((strlen(stateMachine[machineCount-1].str)+2)*sizeof(char));
			stateMachine[machineCount].str=strcpy(stateMachine[machineCount].str,"/");
			stateMachine[machineCount].str=strcat(stateMachine[machineCount].str,stateMachine[machineCount-1].str);
			stateMachine[machineCount].end=stateCount;
			stateMachine[machineCount].isoutput=0;
		}
		token=strtok(NULL,seps);  
		if(token==NULL)
		{
			stateMachine[machineCount-1].isoutput=1;
			stateMachine[machineCount].isoutput=1;
		}
		else machineCount++;
	}
    stateCount++;
}

int split_file(char* file_name,int one_size)
{
	FILE *fp, *fout;
    char nameout[MAX_SIZE];
    int i,j,k,n;
    int size;
    fp = fopen (file_name,"rb");
    if (fp==NULL) { return -1;}
    fseek (fp, 0, SEEK_END);   
    size=ftell (fp);
    rewind(fp);
    n=size/one_size;
    char* buff = (char*)malloc(one_size*sizeof(char));
    for (i=0;i<n;i++)
    {
        sprintf(nameout,"%d.xml",i);
        fout = fopen(nameout,"wb");
        k = fread (buff,1,one_size,fp);
        fwrite(buff,1,one_size,fout);
        fclose(fout);
        printf("output in %s\n",nameout);
    }
    j = size % one_size;
    if (j!=0) {
    sprintf(nameout,"%d.xml",n);
    fout = fopen(nameout,"wb");
    k = fread (buff,1,j,fp);
    fwrite(buff,1,k,fout);
    fclose(fout);
    printf("output in %s\n",nameout);
    
    }
    return n;
}

char* ReadXPath(char* xpath_name)
{
	FILE *fp;
	char* buf=(char*)malloc(MAX_LINE*sizeof(char));
	char* xpath=(char*)malloc(MAX_LINE*sizeof(char));
	xpath=strcpy(xpath,"");
	if((fp = fopen(xpath_name,"r")) == NULL)
    {
        xpath=strcpy(xpath,"error");
    }
    else{
    	while(fgets(buf,MAX_LINE,fp) != NULL)
    	{
    		xpath=strcat(xpath,buf);
		}
	}
	free(buf);
    return xpath;
}

void createTree(int thread_num)
{
	start_root[thread_num]=(Node*)malloc(sizeof(Node));
	finish_root[thread_num]=(Node*)malloc(sizeof(Node));
	start_root[thread_num]->children=(Node**)malloc(stateCount*sizeof(Node*));
	finish_root[thread_num]->children=(Node**)malloc(stateCount*sizeof(Node*));
	finish_root[thread_num]->state=-1;
	int i,j;
	for(i=0;i<=stateCount;i++)
	{
		start_root[thread_num]->children[i]=(Node*)malloc(sizeof(Node));
		finish_root[thread_num]->children[i]=(Node*)malloc(sizeof(Node));
		for(j=0;j<=stateCount;j++)
		{
			start_root[thread_num]->children[i]->children=(Node**)malloc(stateCount*sizeof(Node*));
			finish_root[thread_num]->children[i]->children=(Node**)malloc(stateCount*sizeof(Node*));
			
			start_root[thread_num]->children[i]->children[j]=NULL;
			finish_root[thread_num]->children[i]->children[j]=NULL;
		}
		finish_root[thread_num]->children[i]->hasOutput=0;
		finish_root[thread_num]->children[i]->output=(char*)malloc(MAX_SIZE*sizeof(char));
		finish_root[thread_num]->children[i]->output=strcpy(finish_root[thread_num]->children[i]->output,"");
		finish_root[thread_num]->children[i]->start_node=(Node*)malloc(sizeof(Node));
		start_root[thread_num]->children[i]->finish_node=(Node*)malloc(sizeof(Node));
		start_root[thread_num]->children[i]->state=i;
		start_root[thread_num]->children[i]->parent=start_root[thread_num];
		finish_root[thread_num]->children[i]->state=i;
		finish_root[thread_num]->children[i]->parent=finish_root[thread_num];
		start_root[thread_num]->children[i]->finish_node=finish_root[thread_num]->children[i];
		finish_root[thread_num]->children[i]->start_node=start_root[thread_num]->children[i];
		start_root[thread_num]->children[i]->isLeaf=1;
		finish_root[thread_num]->children[i]->isLeaf=1;
	}
	for(i=0;i<=stateCount;i++)
	{
		for(j=0;j<=stateCount;j++)
		{
			start_root[thread_num]->children[i]->children[j]=NULL;
			finish_root[thread_num]->children[i]->children[j]=NULL;
		}
	}
}

void createTree_first(int start_state)
{
	int thread_num=0;
	start_root[thread_num]=(Node*)malloc(sizeof(Node));
	finish_root[thread_num]=(Node*)malloc(sizeof(Node));
	start_root[thread_num]->children=(Node**)malloc(stateCount*sizeof(Node*));
	finish_root[thread_num]->children=(Node**)malloc(stateCount*sizeof(Node*));
	start_root[thread_num]->isLeaf=0;
	finish_root[thread_num]->isLeaf=0;
	int i,j;
	for(i=0;i<=stateCount;i++)
	{
		if(i==start_state)
		{
			start_root[thread_num]->children[i]=(Node*)malloc(sizeof(Node));
		    finish_root[thread_num]->children[i]=(Node*)malloc(sizeof(Node));
		    finish_root[thread_num]->children[i]->hasOutput=0;
		    finish_root[thread_num]->children[i]->output=(char*)malloc(MAX_SIZE*sizeof(char));
		    finish_root[thread_num]->children[i]->output=strcpy(finish_root[thread_num]->children[i]->output,"");
		    for(j=0;j<=stateCount;j++)
		    {
			    start_root[thread_num]->children[i]->children=(Node**)malloc(stateCount*sizeof(Node*));
			    finish_root[thread_num]->children[i]->children=(Node**)malloc(stateCount*sizeof(Node*));
			    start_root[thread_num]->children[i]->children[j]=NULL;
			    finish_root[thread_num]->children[i]->children[j]=NULL;
		    }
		    finish_root[thread_num]->children[i]->start_node=(Node*)malloc(sizeof(Node));
		    start_root[thread_num]->children[i]->finish_node=(Node*)malloc(sizeof(Node));
		    start_root[thread_num]->children[i]->state=i;
		    start_root[thread_num]->children[i]->parent=start_root[thread_num];
		    finish_root[thread_num]->children[i]->state=i;
		    finish_root[thread_num]->children[i]->parent=finish_root[thread_num];
		    start_root[thread_num]->children[i]->finish_node=finish_root[thread_num]->children[i];
		    finish_root[thread_num]->children[i]->start_node=start_root[thread_num]->children[i];
		    start_root[thread_num]->children[i]->isLeaf=1;
		    finish_root[thread_num]->children[i]->isLeaf=1;
		}
		else
		{
			start_root[thread_num]->children[i]=NULL;
		    finish_root[thread_num]->children[i]=NULL;
		}		
	}
	for(j=0;j<=stateCount;j++)
	{
		start_root[thread_num]->children[start_state]->children[j]=NULL;
		finish_root[thread_num]->children[start_state]->children[j]=NULL;
	}
}

void print_tree(Node* tree,int layer)
{
	QueueEle element;
	Node* node=tree;
	int i=0;
	layer++;
	QueueEle queue[MAX_SIZE];
	int top=-1;
	int buttom=-1;
	int lastlayer=0;
	printf("the 0 layer only includes the root node and its state is -1;");
	for(i=0;i<=stateCount;i++)
	{
		
		if(node->children[i]!=NULL)
		{
		    element.node=node->children[i];
			element.layer=layer;
			queue[(top+1)%MAX_SIZE]=element;
			top=(top+1)%MAX_SIZE;
		}
	}
	while((top-buttom)%MAX_SIZE>0)
	{
		element=queue[(buttom+1)%MAX_SIZE];
		node=element.node;
		layer=element.layer;
		if(layer>lastlayer)
		{
			printf("\nthe %d layer is: ",layer);
			lastlayer=layer;
		}
		printf("state %d  parent %d;",node->state,node->parent->state);
		buttom=(buttom+1)%MAX_SIZE;
		if(node->children!=NULL){
		for(i=0;i<=stateCount;i++)
		{
			if(node->children[i]!=NULL)
		    {
				element.node=node->children[i];
				element.layer=layer+1;
				queue[(top+1)%MAX_SIZE]=element;
				top=(top+1)%MAX_SIZE;
		    }
		}
	}
	}
	printf("\n\n");
}

ResultSet getresult(int n) 
{
	ResultSet final_set,set;
	int i,j,k;
	final_set.topbegin=0;
	final_set.topend=0;
    Node* start_node=start_root[n];
    Node* end_node=finish_root[n];
    final_set.begin=0;final_set.end=0;final_set.output=NULL;final_set.hasOutput=0;
    int start=1;
    Node* root=start_root[0];
    set.begin=start;
    Node* node;
    for(i=0;i<=n;i++)
    {
    	set.begin=start;set.end=0;set.output=NULL;set.hasOutput=0;
    	set.topbegin=0;set.topend=0;
    	node=start_root[i]->children[start];   //the first child for the root
		j=0;
		//deal with the start tree
		if((node==NULL)||(node!=NULL&&node->state>stateCount))
		{
			final_set.begin=-1;
		    break;
	    }
		while(1)
		{
			for(j=0;j<=stateCount;j++)
			{
				if(node->children[j]!=NULL)
				{
			    	node=node->children[j];
			    	set.begin_stack[set.topbegin++]=node->state;
				    break;
			    }
			}
			if(j>stateCount)
			{
				break;
			}
		}
		//deal with final tree
		node=node->finish_node;
		if(node!=NULL&&node->state!=-1)
		{
			set.end_stack[set.topend++]=node->state;
		}
		while(node!=NULL&&node->parent!=NULL&&node->parent->state!=-1)
		{
			node=node->parent;
			set.end_stack[set.topend++]=node->state;
		}
		set.end=set.end_stack[set.topend-1];
		if(node->hasOutput==1&&node->output!=NULL)
		{
			set.output=(char*)malloc((strlen(node->output)+1)*sizeof(char));
			set.output=strcpy(set.output,node->output);
			set.hasOutput=1;
		}
		set.topend--;
		//merge finalset&set
	    if(i>0&&final_set.end!=set.begin)
	    {
		    final_set.begin=-1;
		    break;
	    }
	    else{
		
	        if(set.hasOutput==1)
	        {
		        if(final_set.output==NULL)  {
			        final_set.output=(char*)malloc(MAX_SIZE*sizeof(char));
			        memset(final_set.output,0,sizeof(final_set.output));
		        }
		        if(i>0){
		        	final_set.output=strcat(final_set.output," ");
				}  
		        final_set.output=strcat(final_set.output,set.output);
		        final_set.hasOutput=1;
	        }
	        final_set.end=set.end;
	        if(i==0)
	        {
		        final_set.begin=set.begin;
		        for(k=0;k<set.topbegin;k++)
		        {
			        final_set.begin_stack[final_set.topbegin++]=set.begin_stack[k];
		        }
		        for(k=0;k<set.topend;k++)
		        {
			        final_set.end_stack[final_set.topend++]=set.end_stack[k];
		        }
            }
            else{
	            int equal_flag=0;
	            if(final_set.topend==set.topbegin)
	            {
		            for(k=0;k<set.topbegin;k++)
		            {
			            if(final_set.end_stack[k]!=set.begin_stack[k])
			            {
				            equal_flag=1;
				            break;
			            }
		            }
	            }
                if(equal_flag==0)
                {
    	            for(k=0;k<set.topend;k++)
    	            {
    		            final_set.end_stack[final_set.topend++]=set.end_stack[k];  //merge
		            }
	            }
	            else
	            {
	            	final_set.topend=set.topend;         //end_stack is equal to the current set
	            	for(k=0;k<set.topend;k++)
    	            {
    		            final_set.end_stack[k]=set.end_stack[k];  
		            }
				}
            }

            start=final_set.end;
    	}
	}
	return final_set;
}

void print_result(ResultSet set)
{
	if(set.begin==-1)
	{
		printf("The mapping for this part is null, please check the XPath command.\n");
		return;
	}
	int i;
	printf("The mapping for this part is: %d,  ",set.begin);
	for(i=set.topbegin-1;i>=0;i--)
	{
		printf("%d:",set.begin_stack[set.topbegin--]);
	}
    printf(",  ");
	printf("%d,  ",set.end);
	for(i=set.topend-1;i>=0;i--)
	{
		printf("%d:",set.end_stack[set.topend--]);
	}
	printf(",  ");
	if(set.output!=NULL)  printf("%s\n",set.output);
	else printf("null");
}

void add_node(Node* node, Node* root)  //for finish tree
{
	Node* rtchild;
	int i,j;
    if(root->children[node->state]!=NULL)
    {
         for(i=0;i<=stateCount;i++)
         {
         	if(node->children[i]!=NULL ){
         		rtchild=node->children[i];
         		node->children[i]=NULL;
         		rtchild->parent=NULL;
         		add_node(rtchild,root->children[node->state]);
		    }
         }
    }
    else {
    	root->children[node->state]=node;
    	node->parent=root;
	}
    
}

void push(Node* node, Node* root, int nextState) //push new element into stack
{
    Node* n;
    int i;
    n=(Node*)malloc(sizeof(Node));
    n->state=node->state;
    n->hasOutput=0;
    n->output=(char*)malloc(MAX_LINE*sizeof(char));
    n->output=strcpy(n->output,"");
    n->start_node=(Node*)malloc(sizeof(Node));
    n->finish_node=(Node*)malloc(sizeof(Node));
    n->start_node=NULL;
    n->finish_node=NULL;
    node->state=nextState;
    n->children=node->children;
    if(node->isLeaf==1)
    {
    	node->start_node->finish_node=n;
    	n->start_node=node->start_node;
    	n->isLeaf=1;
    	node->start_node=NULL;
    	node->isLeaf=0;
	}
    for(i=0;i<=stateCount;i++)
    {
    	if(n->children[i]!=NULL)
    	{
    		n->children[i]->parent=n;
		}
	}
    node->start_node=NULL;
    node->children=NULL;
    node->children=(Node**)malloc(stateCount*sizeof(Node*));
    node->children[n->state]=n;
    n->parent=node;
 
    for(i=0;i<=stateCount;i++ )
    {
    	if(i!=n->state)
    	   node->children[i]=NULL;
	}

    add_node(node,root);
}

int checkChildren(Node* node)  //return value--the number of children -1--no child
{
	int i;
	for(i=0;i<=stateCount;i++)
	{
		if(node->children[i]!=NULL)
		{
			break;
		}
	}
	if(i<=stateCount)  return i;
	else return -1;
}

void pop(char * str, Node* root) //pop element due to end_tag e.g</d>
{
    int i,j,begin,next;
    int flag=0;
    Node * n;
    int isoutput=0;
    int k;
    for(j=machineCount;j>=1;j--)
    {
        if(strcmp(str,stateMachine[j].str)==0)
        {
            break;
		}
	}
	if(j>=1)
	{
		begin=stateMachine[j].start;
		next=stateMachine[j].end;
		isoutput=stateMachine[j].isoutput;
		for(j=1;j<=stateCount;j++)  
		{
			if(root->children[j]!=NULL)
			{
				if(root->children[j]->state==begin)
			        break;
			}
		}
		if(j<=stateCount)
		{
			n=root->children[j]->children[next];  //for state j
			if(n!=NULL&&n->state==next)
			{
				if(isoutput==1)
				{
					if(n->hasOutput==1)
					    n->output=strcat(n->output," ");
					else n->hasOutput=1;
					n->output=strcat(n->output,root->children[j]->output);
					root->children[j]->hasOutput=0;
				}
				n->parent=NULL;
				root->children[j]->children[next]=NULL;
				add_node(n,root);

				if(checkChildren(root->children[j])==-1)
				{
					if(root->children[j]->start_node!=NULL)  free(root->children[j]->start_node);
					root->children[j]->parent=NULL;
					if(root->children[j]) free(root->children[j]);
					root->children[j]=NULL;
					flag=1;
				}
				
				if(root->children[0]!=NULL)
				{
					for(i=stateCount;i>=0;i--)  //for state0
				    {
				        	
					    if(root->children[0]->children[i]!=NULL)
					    {

					        n=root->children[0]->children[i];
					        n->parent=NULL;
					        root->children[0]->children[i]=NULL;
					        if(i==0){
					            root->children[0]->parent=NULL;
					            root->children[0]=NULL;
							}
					        add_node(n,root);
						}
			     	}
			   } 
			}
			else if(flag==0) //not in final tree, add it into the start tree
			   {
			   	    n=root->children[begin]->start_node;
			   	    if(n!=NULL)
				    {
				    	if(root->children[next]->start_node!=NULL)
				    	{
				            Node* ns=(Node*)malloc(sizeof(Node));
                            ns->state=begin;
                            ns->parent=n->parent;
                            ns->children=NULL;
                            ns->start_node=NULL;
				    		n->children[next]=root->children[next]->start_node;  //for pop node
				    		root->children[next]->start_node->parent->children[next]=NULL;
                            root->children[next]->start_node->parent=n;
                            ns->finish_node=(Node*)malloc(sizeof(Node));
                            ns->finish_node=root->children[begin];
                            if(root->children[begin]->hasOutput==1)
				            {
					            root->children[next]->hasOutput=1;
					            root->children[next]->output=strcpy(ns->finish_node->output,root->children[begin]->output);
					            root->children[begin]->hasOutput=0;
				            }
                            root->children[begin]->start_node=ns;
                            //for pop node 0
                            n=root->children[0]->start_node;
                            n->children=(Node**)malloc(sizeof(Node));
                            for(k=0;k<=stateCount;k++)
                            {
                            	n->children[k]=NULL;
							}
                            ns=NULL;
                            
                            if(n!=NULL)
                            {
                            	for(i=0;i<=stateCount;i++)
                            	{
                            		if(i==next)
									{
										continue;
									} 
                            		if(i==0)
                            		{
                            			ns=(Node*)malloc(sizeof(Node));
                                        ns->state=i;
                                        ns->parent=n;
                                        ns->children=NULL;
                                        ns->start_node=NULL;
                                        n->children[i]=ns;
                                        ns->finish_node=(Node*)malloc(sizeof(Node));
                                        ns->finish_node=root->children[0];
                                        root->children[0]->start_node=ns;
									}
										
									if(i!=0){
                                        if(i!=begin)
										{
                                        	root->children[i]->start_node->parent->children[i]=NULL;
										}
                                        root->children[i]->start_node->parent=n;
										n->children[i]=root->children[i]->start_node;  //for next state
                                    }
								}
							}
						}
					}
				}
		}
	}      
}

char * convertTokenTypeToStr(xml_TokenType type)
{

	if(type<MAX_ATT_NUM) return tokenValue[type];
	else return defaultToken;
}

int xml_initText(xml_Text *pText, char *s)
{
    pText->p = s;
    pText->len = strlen(s);
    return 0;
}

int xml_initToken(xml_Token *pToken, xml_Text *pText)
{
    pToken->text.p = pText->p;
    pToken->text.len = 0;
    pToken->type = xml_tt_U;
    return 0;
}

int xml_print(xml_Text *pText, int begin, int end)
{
    int i;
    char * temp=pText->p;
    temp = ltrim(pText->p);
    int j=0;
    for (i = begin; i < end; i++)
    {
        putchar(temp[i]);
    }
    return 0;
}

char* xml_substring(xml_Text *pText, int begin, int end)
{
    int i,j;
    char * temp=pText->p;
    temp = ltrim(pText->p);
    char* temp1=(char*)malloc((end-begin+1)*sizeof(char));
    char t;
    for (j = 0,i = begin; i < end; i++,j++)
    {
        temp1[j]=temp[i];
    }
    temp1[j]='\0';
    return temp1;
}

char* substring(char *pText, int begin, int end)
{
    int i,j;
    char * temp=pText;
    temp = ltrim(pText);
    char  des[end-begin+1];
    char* temp1=(char*)malloc((end-begin+1)*sizeof(char));
    for (j = 0,i = begin; i < end; i++,j++)
    {
        temp1[j]=temp[i];
    }
    temp1[j]='\0';
    return temp1;
}

int xml_println(xml_Text *pText)
{
    xml_print(pText, 0 , pText->len);
    putchar('\n');
    return 0;
}

char * ltrim(char *s)
{
     char *temp;
     temp = s;
     while((*temp == ' ' || *temp=='\t' )&&temp){*temp++;}
     return temp;
}


int xml_process(xml_Text *pText, xml_Token *pToken, int multilineExp, int multilineCDATA, int thread_num)   //return value:0--success -1--error 1--multiline explantion 2--multiline CDATA
{
	Node * tempnode=finish_root[thread_num];
    char *start = pToken->text.p + pToken->text.len;
    char *p = start;
    char *end = pText->p + pText->len;
    int state = 0;
    int templen = 0;
    if(multilineExp == 1) state = 10;   //1--multiline explantion  0--single line explantion
    if(multilineCDATA == 1) state = 17; //1--multiline CDATA 0--single CDATA
    int j,a;
    Node* node;

    pToken->text.p = p;
    pToken->type = xml_tt_U;
    
    for (; p < end; p++)
    {
        switch(state)
        {
            case 0:
               switch(*p)
               {
                   case '<':
                   	   
                       state = 1;
                       break;
                   case ' ':
                   	   state = 0;
                   	   break;
                   default:
                       state = 7;
                       break; 
               }
            break;
            case 1:
               switch(*p)
               {
                   case '?':
                       state = 2;
                       break;
                   case '/':
                       state = 4;
                       break;
                   case '!':
                   	   state = 8;
                   	   break;
                   case ' ':
                   	   state = -1;
                   	   break;
                   default:
                       state = 5;
                       break;
               }
            break;
            case 2:
               switch(*p)
               {
                   case '?':
                       state = 3;
                       break;
                   default:
                       state = 2;
                       break;
               }
            break;
            case 3:
               switch(*p)
               {
               	   putchar(*p);
                   case '>':                        /* Head <?xxx?>*/
                       pToken->text.len = p - start + 1;
                       pToken->type = xml_tt_H;
                       //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
                       //printf("%s","content=");
                       templen = pToken->text.len;
                       pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                       //xml_print(&pToken->text, 0 ,pToken->text.len);
                       //printf(";\n\n");
                       pToken->text.p = start + templen;
                       start = pToken->text.p;
                       state = 0;
                       break;
                   default:
                       state = -1;
                       break;
               }
               break;
            case 4:
                switch(*p)
                {
                   case '>':              /* End </xxx> */
                       pToken->text.len = p - start + 1;
                       pToken->type = xml_tt_E;
                       //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
                       //printf("%s","content=");
                       
                       templen = pToken->text.len;
                       pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                       //xml_print(&pToken->text, 2 , pToken->text.len-1);
                       //printf(";\n\n");
                       
                       char* subs=xml_substring(&pToken->text , 1 , pToken->text.len-1);
                       pop(subs,finish_root[thread_num]);
                       pToken->text.p = start + templen;
                       start = pToken->text.p;
                       //layer--;
                       state = 0;
                       break;
                   case ' ':
                   	   state = -1;
                   	   break;
                   default:
                       state = 4;
                       break;
                }
                break;
            case 5:
                switch(*p)
                {
                   case '>':               /* Begin <xxx> */
                       pToken->text.len = p - start + 1;
                       pToken->type = xml_tt_B;
                       if(pToken->text.len-1 >= 1){
                       	   //layer++;
                       	   //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
                           //printf("%s","content=");
                           templen = pToken->text.len;
                           pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                           
                       	   //xml_print(&pToken->text , 1 , pToken->text.len-1);
                       	   
                           //printf(";\n\n");
                           char* sub=xml_substring(&pToken->text , 1 , pToken->text.len-1);
                           
                           if(currentStr[thread_num]!=NULL) free(currentStr[thread_num]);
						   currentStr[thread_num]=(char*)malloc((strlen(sub)+1)*sizeof(char));
						   currentStr[thread_num]=strcpy(currentStr[thread_num],sub);
                           for(j=1;j<=machineCount;j++)
                           {
                           	   if(strcmp(sub,stateMachine[j].str)==0)
                           	   {
                           	      break;
							   }
						   }

						   if(j<=machineCount)   
						   {
						   	    int a;
						   	    for(a=0;a<=stateCount;a++)  //for state0
						   	    {
						   	    	if(a!=stateMachine[j].start&&finish_root[thread_num]->children[a]!=NULL)
						   	    	{
						   	    		node=finish_root[thread_num]->children[a];
                                        finish_root[thread_num]->children[a]=NULL;
                                        push(node,finish_root[thread_num],0);
									}
								}
								int begin=stateMachine[j].start;
								int end=stateMachine[j].end;
								node=finish_root[thread_num]->children[begin];
								finish_root[thread_num]->children[begin]=NULL;
								push(node,finish_root[thread_num],end);   //for state j								
						   }
                           
					   }
					   else templen = 1;
                       pToken->text.p = start + templen;
                       start = pToken->text.p;
                       state = 0;
                       break;
                   case '/':
                       state = 6;
                       break;
                   case ' ':                 /* Begin <xxx> */
                   	   pToken->text.len = p - start + 1;
                   	   templen = 0;
                       pToken->type = xml_tt_B;
                       if(pToken->text.len-1 >= 1)
                       {
                       	   //layer++;
                       	   //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
                       	   //printf("%s","content=");
                       	   templen = pToken->text.len;
                       	   pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                       	   //xml_print(&pToken->text , 1 , pToken->text.len-1);
                       	   //printf(";\n\n");
                       	   char* sub=xml_substring(&pToken->text , 1 , pToken->text.len-1);                          
                           if(currentStr[thread_num]!=NULL) free(currentStr[thread_num]);
						   currentStr[thread_num]=(char*)malloc((strlen(sub)+1)*sizeof(char));
						   currentStr[thread_num]=strcpy(currentStr[thread_num],sub);
                           for(j=1;j<=machineCount;j++)
                           {
                           	   if(strcmp(sub,stateMachine[j].str)==0)
                           	   {
                           	      break;
							   }
						   }
						   if(j<=machineCount)   
						   {
						   	    int a;
						   	    for(a=0;a<=stateCount;a++)  //for state0
						   	    {
						   	    	if(a!=stateMachine[j].start&&finish_root[thread_num]->children[a]!=NULL)
						   	    	{
						   	    		node=finish_root[thread_num]->children[a];
                                        finish_root[thread_num]->children[a]=NULL;
                                        push(node,finish_root[thread_num],0);
									}
								}
								int begin=stateMachine[j].start;
								int end=stateMachine[j].end;
								node=finish_root[thread_num]->children[stateMachine[j].start];
								finish_root[thread_num]->children[stateMachine[j].start]=NULL;
								push(node,finish_root[thread_num],stateMachine[j].end);   //for state j
								currentStr[thread_num]=(char*)malloc((strlen(sub)+1)*sizeof(char));
								currentStr[thread_num]=strcpy(currentStr[thread_num],sub);
						   }
					   }
					    
                       pToken->text.p = start + templen;
                       start = pToken->text.p;
                   	   state = 13;
                   	   break;
                   default:
                       state = 5;
                   break;
                }
                break;
            case 6:
                switch(*p)
                {
                   case '>':   /* Begin End <xxx/> */
                       pToken->text.len = p - start + 1;
                       pToken->type = xml_tt_BE;
                       //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer+1);
                       //printf("%s","content=");
                       templen = pToken->text.len;
                       pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                       //xml_print(&pToken->text , 1 , pToken->text.len-2);
                       //printf(";\n\n");
                       pToken->text.p = start + templen;
                       start = pToken->text.p;
                       state = 0;
                       break;
                      //return 1;
                   default:
                       state = -1;
                   break;
                } 
                break;
            case 7:
                switch(*p)
                {
                   case '<':     /* Text xxx */
                       p--;
                       pToken->text.len = p - start + 1;
                       pToken->type = xml_tt_T;
                       
                       //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
                       //printf("%s","content=");
                       
                       templen = pToken->text.len;
                       pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                       
                       //xml_print(&pToken->text, 0 , pToken->text.len);
                       //printf(";\n\n");
                       char* sub=xml_substring(&pToken->text , 0 , pToken->text.len);
                       pToken->text.p = start + templen;
                       start = pToken->text.p;
                       state = 0;
                       
                       for(j=1;j<stateCount;j++)
                       {
                       	   if(finish_root[thread_num]->children[j]!=NULL)
                       	   {
							    break;
					       }
					   }
					   if(2*(tempnode->children[j]->state-1)>0){
					       char * cmpstr=substring(stateMachine[2*(tempnode->children[j]->state-1)].str,1,strlen(stateMachine[2*(tempnode->children[j]->state-1)].str));
					       if(strcmp(cmpstr,currentStr[thread_num])==0&&stateMachine[2*(finish_root[thread_num]->children[j]->state-1)].isoutput==1)
					       {
					       	   if(tempnode->children[j]->hasOutput==1)
								  tempnode->children[j]->output=strcat(tempnode->children[j]->output," ");
					           else tempnode->children[j]->hasOutput=1;
					           tempnode->children[j]->output=strcat(tempnode->children[j]->output,sub);
					       }
				       }
                       break;
                   default:
                       state = 7;
                       break;
                }
                break;
            case 8:
            	switch(*p)
            	{
            		case '-':
            			state = 9;
            			break;
            		case '[':
            			if(*(p+1)=='C'&&*(p+2)=='D'&&*(p+3)=='A'&&*(p+4)=='T'&&*(p+5)=='A')
            			{
            				state = 16;
            				p += 5;
            				break;
						}
						else
						{
							state = -1;
							break;
						}
            		default:
            			state = -1;
            			break;
				}
			    break;
			case 9:
				switch(*p)
				{
					case '-':
						state = 10;
						break;
					default:
						state = -1;
						break;
				}
			    break;
			case 10:
				switch(*p)
				{
					case '-':
						state = 11;
						break;
					default:
						state = 10;
						break;
				}
			    break;
			case 11:
				switch(*p)
				{
					case '-':
						state = 12;
						break;
					default:
						state = -1;
						break;
				}
			    break;
			case 12:
				switch(*p)
				{
					case '>':                            /* Comment <!--xx-->*/
					    pToken->text.len = p - start + 1;
                        pToken->type = xml_tt_C;
                        //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
                        //printf("%s","content=");
                        templen = pToken->text.len;
                        pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                        if(multilineExp == 1)
                        {
                        	strcat(multiExpContent[thread_num],pToken->text.p);
                            //printf("%s",ltrim(multiExpContent[thread_num]));
                            //printf("\n");
                            memset(multiExpContent[thread_num], 0 , sizeof(multiExpContent[thread_num]));
						}
						/*else{
							xml_print(&pToken->text , 0 , pToken->text.len);
                            printf(";\n\n");
						}*/
						
                        pToken->text.p = start + templen;
                        start = pToken->text.p;
                        state = 0;
						break;
					default:
						state = -1;
						break;
				}
			    break;
			case 13:
				switch(*p)
				{
					case '>':
						state = -1;
						break;
					case '=':                       /*attribute name*/
					    pToken->text.len = p - start + 1;
                        pToken->type = xml_tt_ATN;
                        //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
                        //printf("%s","content=");
                        templen = pToken->text.len;
                        pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                        //xml_print(&pToken->text, 0 , pToken->text.len-1);
                        //printf(";\n\n");
                        pToken->text.p = start + templen;
                        start = pToken->text.p;
						state = 14;
						break;
					default:
						state = 13;
						break;
				}
			    break;
			case 14:
				switch(*p)
				{
					case '"':                                       
                   	    state = 15;
						break;
					case ' ':
						state = 14;
						break;
					default:
						state = -1;
						break;
				}
			    break;	
			case 15:
				switch(*p)
				{
					case '"':                        /*attribute value*/
						pToken->text.len = p - start + 1;
                        pToken->type = xml_tt_ATV;
                        //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
                        //printf("%s","content=");
                        templen = pToken->text.len;
                        pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                        //xml_print(&pToken->text, 1 , pToken->text.len-1);
                        //printf(";\n\n");
                        pToken->text.p = start + templen;
                        start = pToken->text.p;
                        state = 5;
						break;
					default:
						state = 15;
						break;
				}
			    break;
			case 16:
				switch(*p)
				{
					case '[':                                       
                   	    state = 17;
						break;
					default:
						state = -1;
						break;
				}
			    break;	
			case 17:
				switch(*p)
				{
					case ']':                                       
                   	    state = 18;
						break;
					default:
						state = 17;
						break;
				}
			    break;	
			case 18:
				switch(*p)
				{
					case ']':                                       
                   	    state = 19;
						break;
					default:
						state = -1;
						break;
				}
			    break;	
			case 19:
				switch(*p)
				{
					case '>':                                       
                   	    pToken->text.len = p - start + 1;
                        pToken->type = xml_tt_CDATA;
                        //printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
                        //printf("%s","content=");
                        templen = pToken->text.len;
                        pToken->text.len -= strlen(pToken->text.p)-strlen(ltrim(pToken->text.p));
                        if(multilineCDATA == 1)
                        {
                        	strcat(multiCDATAContent[thread_num],pToken->text.p);
                        	/*for( j = 10;j < strlen(multiCDATAContent[thread_num]) - 3;j++)
                        	{
                        		if(multiCDATAContent[thread_num][j] == ']' && multiCDATAContent[thread_num][j+1] == ']') break;
                        		putchar(multiCDATAContent[thread_num][j]);
							}
                            printf("\n");*/
                            memset(multiCDATAContent[thread_num], 0 , sizeof(multiCDATAContent[thread_num]));
						}
						/*else{
							xml_print(&pToken->text , 9 , pToken->text.len-3);
                            printf(";\n\n");
						}*/
						
                        pToken->text.p = start + templen;
                        start = pToken->text.p;
                        state = 0;
						break;
					default:
						state = -1;
						break;
				}
			    break;	
				
            default:  
                state = -1;
                break;
        }
    }
    if(state==-1) {return -1;}
    else if(state == 10)
	{
		strcat(multiExpContent[thread_num],ltrim(pToken->text.p));
		return 1;
	} 
	else if(state == 17)
	{
		strcat(multiCDATAContent[thread_num],ltrim(pToken->text.p));
		return 2;
	}
	else if(state == 7)
	{
		p--;
        pToken->text.len = p - start + 1;
        if(pToken->text.len>1)
        {
        	//printf("type=%s;  depth=%d;  ", convertTokenTypeToStr(pToken->type) , layer);
            //printf("%s","content=");
            //xml_print(&pToken->text, 0 , pToken->text.len);
            //printf(";\n\n");
            char* sub=xml_substring(&pToken->text , 0 , pToken->text.len);
            for(j=1;j<=stateCount;j++)
            {
                if(finish_root[thread_num]->children[j]!=NULL)
                {
                    if(finish_root[thread_num]->children[j]->state<=stateCount)
                    {
					    break;
				    }       	   	   
			    }
		    }
		    if(2*(finish_root[thread_num]->children[j]->state-1)>0){
		        char * cmpstr=substring(stateMachine[2*(finish_root[thread_num]->children[j]->state-1)].str,1,strlen(stateMachine[2*(finish_root[thread_num]->children[j]->state-1)].str));
					   
		        if(strcmp(cmpstr,currentStr[thread_num])==0&&stateMachine[2*(finish_root[thread_num]->children[j]->state-1)].isoutput==1)
		        {
			        finish_root[thread_num]->children[j]->hasOutput=1;
			        finish_root[thread_num]->children[j]->output=strcat(finish_root[thread_num]->children[j]->output,sub);
		        }            
     	    }
        }
		return 0;
	}
    else return 0;
}

void *main_thread(void *arg)
{
	int ret = 0;
    xml_Text xml;
    xml_Token token;    
    char buf[MAX_LINE];  
    char nameout[MAX_SIZE];  //save with file name
    FILE *fp;           
    int len;            
    int multiExp = 0; //0--single line explanation 1-- multiline explanation
    int multiCDATA = 0; //0--single line CDATA 1-- multiline CDATA
    int i=(int)(*((int*)arg));
    if(i==0) 
	{
		createTree_first(1);
	}
    else {
    	createTree(i);
	}
    finish_root[i]->state=-1;
    start_root[i]->state=-1;
    printf("The initial stack tree for the thread %d is shown as follows.\n",i);
	printf("For the start tree\n");
	print_tree(start_root[i],0);
    printf("For the finish tree\n");
    print_tree(finish_root[i],0);
    currentStr[i]=NULL;  //save current node name
    sprintf(nameout,"%d.xml",i);
    if((fp = fopen(nameout,"r")) == NULL)
    {
        printf("There are something wrong with the temp file, we can not load your file. The file name is %s.",nameout);
        exit (1) ;
    }
    //printf("The results are listed as follows:\n");
    while(fgets(buf,MAX_LINE,fp) != NULL)
    {
        len = strlen(buf);
        xml_initText(&xml,buf);
        xml_initToken(&token, &xml);
        
        ret = xml_process(&xml, &token, multiExp, multiCDATA, i);
        
        if(ret == -1)
        {
        	printf("WARNING: There is something wrong with your xml file, please check its format! The file name is %s.",nameout);
        	break;
		}
		else if(ret == 1)
		{
        	multiExp = 1;
		}
		else if(ret == 2)
		{
			multiCDATA = 1;
		}
		else{
			multiExp = 0;
			multiCDATA = 0;
		}
		
    }

    printf("The final stack tree for the thread %d is shown as follows.\n",i);
	printf("For the start tree\n");
	print_tree(start_root[i],0);
    printf("For the finish tree\n");
    print_tree(finish_root[i],0);
    fclose(fp);
	return NULL;
}



void thread_wait(int n)
{
	int t;
	for( t = 0; t <= n; t++)
	{
		pthread_join(thread[t], NULL);
	}
        
}

int main()
{
	
    int ret = 0;
    int one_size=320;
    char* file_name=malloc(50*sizeof(char));
    file_name=strcpy(file_name,"test.xml");
    char * xpath_name=malloc(50*sizeof(char));
    xpath_name=strcpy(xpath_name,"XPath.txt");
    printf("Welcome to the XML lexer program! Your file name is test.xml\n\n");
    int n=split_file(file_name,one_size);    //split file into several parts
    
    if(n==-1)
    {
    	printf("There are something wrong with the xml file, we can not load it. Please check whether it is placed in the right place.");
    	exit(1);
	}
	
	char* xmlPath="/company/develop/programmer";
	xmlPath=ReadXPath(xpath_name);
	if(strcmp(xmlPath,"error")==0)
	{
		printf("There are something wrong with the XPath file, we can not load it. Please check whether it is placed in the right place.");
    	exit(1);
	}
    createAutoMachine(xmlPath);     //create auto machine by xmlpath
    printf("The basic structure of the automata is (from to end):\n");
    int i,rc;
    char *out=" is an output";
    for(i=1;i<=machineCount;i=i+2)
    {
    	if(i==1){
    		printf("%d",stateMachine[i].start);
		}
		printf(" (str:%s",stateMachine[i].str);
		if(stateMachine[i].isoutput==1)
		{
			printf("%s",out);
		}
		printf(") %d",stateMachine[i].end);
	}
	printf("\n");
	for(i=machineCount;i>0;i=i-2)
    {
    	if(i==machineCount){
    		printf("%d (str:%s) %d",stateMachine[i].start,stateMachine[i].str,stateMachine[i].end);
		}
		else
		{
			printf(" (str:%s) %d",stateMachine[i].str,stateMachine[i].end);
		}	
	}
	printf("\n\n");
    
    for(i=0;i<=n;i++)
    {
    	thread_args[i]=i;
    	rc=pthread_create(&thread[i], NULL, main_thread, &thread_args[i]);  //parallel xml processing
        
    	if (rc)
        {
            printf("ERROR; return code is %d\n", rc);
            return EXIT_FAILURE;
        }
	}
	thread_wait(n);
    printf("\n");
	printf("All the subthread ended, now the program is merging its results.\n");
	ResultSet set=getresult(n);
	printf("The mappings for text.xml is:\n");
	print_result(set);
    
    return 0;
}
