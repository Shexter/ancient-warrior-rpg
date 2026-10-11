
#define cls system("cls")
#define pause system("pause")

int menu(string arrayMenu[],int size){
    for(int i=0;i<size;i++){
        cout<<i+1<<"."<<setw(13)<<left<<arrayMenu[i]<<'\n';
        Sleep(100);
    }
    if(arrayMenu[size-1]!="EXIT"){
        cout<<size+1<<".EXIT\n";
    }
    prncol(3);
    cout<<"SELECT: ";
    prncol(7);
    int ret;
    cin>>ret;
    return ret;
}
int menu2(string arrayMenu[],int size,string array2Menu[]){
    for(int i=0;i<size;i++){
        cout<<i+1<<"."<<setw(13)<<arrayMenu[i]<<" - "<<array2Menu[i]<<'\n';
        Sleep(100);
    }
    if(arrayMenu[size-1]!="EXIT"){
        cout<<size+1<<".EXIT\n";
    }
    cout<<"SELECT: ";
    int ret;
    cin>>ret;
    return ret;
}
int menu3(int array[],int size,string array1[],int array2[],string array3[]){
    for(int i=0;i<size;i++){
        cout<<i+1<<".";
        if(array[i]==0) cout<<'\n';
        else cout<<left<<setw(13)<<array1[i]<<"("<<setw(3)<<right<<array2[i]<<") : "<<array3[i]<<'\n';
        Sleep(100);
    }
    if(array1[size-1]!="EXIT"){
        cout<<size+1<<".EXIT\n";
    }
    cout<<"SELECT: ";
    int ret;
    cin>>ret;
    return ret;
}

void showBar(int delay){
    for(int i=0;i<10;i++){
        cout<<'\r';
        for(int j=0;j<=i;j++) cout<<char(178);
        cout<<rEPL(" ",10-i);
        cout<<(i+1)*10<<" %";
        Sleep(delay);
    }
}
bool yesno(string str){
    char yn;
    cout<<str<<" (Y/N)?";
    cin>>yn;yn=toupper(yn);
    if(yn=='Y') return 1;
    else if(yn=='N') return 0;
    else {cout<<"Y or N";Sleep(2000);return 0;}
}
void storyLine(string line[],int size,int speed){
    for(int i=0;i<size;i++){
        for(int j=0;j<line[i].length();j++){
            cout<<line[i][j];
            Sleep(speed);
        }
        Sleep(200);
        cout<<'\n';
    }
}
void showTitle(int delay=0){
    cls;
    string title[1]={"Ancient Warrior RPG"};
    prncol(4);
    storyLine(title,1,delay);
    prncol(7);
}
int zeroint(int x){
    if(x<0) return 0;
    else return x;
}
void strarr2File(string filename,string array[],int size){
    fstream file(filename,ios::out);
    for(int i=0;i<size;i++) file<<array[i]<<'\n';
    file.close();
}
void intarr2File(string filename,int array[],int size){
    fstream file(filename,ios::out);
    for(int i=0;i<size;i++) file<<array[i]<<'\n';
    file.close();
}
void file2Arrstr(string filename,string array[],int size){
    fstream file(filename,ios::in);
    for(int i=0;i<size;i++) getline(file,array[i]);
    file.close();
}
void file2Arrint(string filename,int array[],int size){
    fstream file(filename,ios::in);
    for(int i=0;i<size;i++) file>>array[i];
    file.close();
}
void hero2File(string heroName,int heroMaxMP,int heroMP,int heroMaxHP,int heroHP,int heroDMG,int heroDEF,int heroLIFE,int heroLVL,int heroEXP,int heroTNL,int heroPath,int heroGold,int weaponID,int armorID,int weaponUP,int armorUP,bool heroBank){
    fstream file("game0.dat",ios::out);
    file<<heroName<<'\n';
    file<<heroMaxMP<<'\n';
    file<<heroMP<<'\n';
    file<<heroMaxHP<<'\n';
    file<<heroHP<<'\n';
    file<<heroDMG<<'\n';
    file<<heroDEF<<'\n';
    file<<heroLIFE<<'\n';
    file<<heroLVL<<'\n';
    file<<heroEXP<<'\n';
    file<<heroTNL<<'\n';
    file<<heroPath<<'\n';
    file<<heroGold<<'\n';
    file<<weaponID<<'\n';
    file<<armorID<<'\n';
    file<<weaponUP<<'\n';
    file<<armorUP<<'\n';
    file<<heroBank<<'\n';
    file.close();

    fstream file2("game00.dat",ios::out);
    file2<<heroName;
    file2.close();
}
void file2Hero(string &heroName,int &heroMaxMP,int &heroMP,int &heroMaxHP,int &heroHP,int &heroDMG,int &heroDEF,int &heroLIFE,int &heroLVL,int &heroEXP,int &heroTNL,int &heroPath,int &heroGold,int &weaponID,int &armorID,int &weaponUP,int &armorUP,bool &heroBank){
    fstream file("game0.dat",ios::in);
    file>>heroName;
    file>>heroMaxMP;
    file>>heroMP;
    file>>heroMaxHP;
    file>>heroHP;
    file>>heroDMG;
    file>>heroDEF;
    file>>heroLIFE;
    file>>heroLVL;
    file>>heroEXP;
    file>>heroTNL;
    file>>heroPath;
    file>>heroGold;
    file>>weaponID;
    file>>armorID;
    file>>weaponUP;
    file>>armorUP;
    file>>heroBank;
    file.close();
}
string pathName(int path){
    string name;
    if(path==1) name="FIGHTER";
    else if(path==2) name="HUNTER";
    else name="ROGUE";

    return name;
}
void createHero(int path,int &maxmp,int &maxhp,int &att,int &def,int &lif,int &tnl){
    cout<<"Creating a "<<pathName(path)<<" Warrior...\n";
    switch(path){
        case 1:{maxmp=50;maxhp=350;att=50;def=5;lif=0;break;}
        case 2:{maxmp=100;maxhp=200;att=100;def=5;lif=10;break;}
        case 3:{maxmp=200;maxhp=100;att=175;def=25;lif=10;break;}
    }
    tnl=100*path;  //first tnl
    showBar(200);
}
void showStr(string text, int width,char chFill=' ',char LR='l'){
    LR=toupper(LR);
    cout<<setfill(chFill)<<setw(width);
    if(LR='L') cout<<left<<text;
    else cout<<right<<text;

}
void showNum(int num, int width,char chFill=' ',char LR='r'){
    LR=toupper(LR);
    cout<<setfill(chFill)<<setw(width);
    if(LR='R') cout<<right<<num;
    else cout<<left<<num;
}

string id2Name(int id,int itemID[],string itemName[]){
    string name="";
    for(int i=0;i<20;i++) {
        if(id==itemID[i]) {name=itemName[i];break;}
    }
    return name;
}
void showAttr(int id,int itemID[],int itemAttr[][3]){
    for(int i=0;i<20;i++) {
        if(id==itemID[i]) {
            for(int j=0;j<3;j++){
                showNum(itemAttr[i][j],4);
                if(j<2) cout<<'|';//99|99|99 item attr
            }
            break;
        }
    }
}
int id2Attr(int id,int itemID[],int itemAttr[][3],int idx){
    for(int i=0;i<20;i++) {
        if(itemID[i]==id) {
            return itemAttr[i][idx];
        }
    }
    return 0;
}
int eq2Attr(int weaponID,int armorID,int weaponUP,int armorUP,int idx,int itemID[],int itemAttr[][3]){
    int ret=0;
    for(int i=0;i<20;i++) {
        if(weaponID==itemID[i]) {
            ret+=itemAttr[i][idx]*weaponUP;
        }
        if(armorID==itemID[i]) {
            ret+=itemAttr[i][idx]*armorUP;
        }
    }
    return ret;
}
string enkrip(string str){
    int len=str.length();
    char crnd=rand()%10+70;
    string nkripstr="\0";
    nkripstr+=crnd;
    for(int i=0;i<len;i++){
        nkripstr+=(str[i]+crnd);
    }
    return nkripstr;
}
string dekrip(string str){
    int len=str.length();
    char rnd=str[0];
    str=str.substr(1,len-1);
    string dkripstr="\0";
    for(int i=0;i<str.length();i++){
        dkripstr+=(str[i]-rnd);
    }
    return dkripstr;
}
void hpmpBar(int hpmp,int maxhpmp){
    int pointbar=hpmp/(maxhpmp/10);
    int maxpointbar=10-pointbar;
    cout<<rEPL(char(178),pointbar);
    cout<<rEPL(char(176),maxpointbar);
}
void heroStat2(string name,int maxmp,int mp,int maxhp,int hp,int dmg,int def,int life, int lv,int exp,int tnl,int path,int gold,int weaponID,int armorID,int weaponUP,int armorUP,int itemID[],string itemName[],int itemAttr[][3]){
    prncol(3);
    cout<<pathName(path)<<" : "<<setw(8)<<right<<name;
    prncol(7);
    cout<<"  LEVEL  : ";showNum(lv,5);
    cout<<"\nEXP    : ";showNum(exp,5);
    cout<<"\nTNL    : ";hpmpBar(tnl,100*path*lv);cout<<' '<<tnl<<'/'<<100*path*lv;
    cout<<"\nHP     : ";hpmpBar(hp,maxhp);cout<<' '<<hp<<'/'<<maxhp;
    cout<<"\nMP     : ";hpmpBar(mp,maxmp);cout<<' '<<mp<<'/'<<maxmp;
    cout<<"\nDMG    : ";showNum(dmg,5);
    cout<<"  DEF    : ";showNum(def,5);
    cout<<"\nLIFE   : ";showNum(life,5);
    cout<<"  Gold   : ";showNum(gold,5);
    cout<<"\nWeapon : ";
    if(weaponID==0) cout<<"No Weapon equipped";
    else{
        showAttr(weaponID,itemID,itemAttr);
        cout<<" "<<id2Name(weaponID,itemID,itemName)<<" Lv"<<weaponUP;
    }
    cout<<"\nArmor  : ";
    if(armorID==0) cout<<"No Armor equipped\n\n";
    else{
        showAttr(armorID,itemID,itemAttr);
        cout<<" "<<id2Name(armorID,itemID,itemName)<<" Lv"<<armorUP<<"\n\n";
    }
}

void heroStat1(string name,int maxmp,int mp,int maxhp,int hp,int dmg,int def,int life,int lv,int exp,int tnl,int path,int gold){
    prncol(3);
    cout<<pathName(path)<<" : "<<name<<' ';
    prncol(7);
    if(lv!=0)   cout<<"Level   : "<<setw(5)<<lv<<'\n';
    if(hp!=0)   {cout<<"HP      : ";hpmpBar(hp,maxhp);showNum(hp,5);cout<<'/'<<maxhp<<'\n';}
    if(mp!=0)   {cout<<"MP      : ";hpmpBar(mp,maxmp);showNum(mp,5);cout<<'/'<<maxmp<<'\n';}
    if(exp!=0)  cout<<"EXP     : "<<setw(5)<<exp<<" | ";
    if(dmg!=0)  cout<<"Damage  : "<<setw(5)<<dmg<<'\n';
    if(tnl!=0)  cout<<"TNL     : "<<setw(5)<<tnl<<" | ";
    if(def!=0)  cout<<"Defense : "<<setw(5)<<def<<'\n';
    if(gold!=0) cout<<"Gold    : "<<setw(5)<<gold<<" | ";
    if(life!=0) cout<<"Life    : "<<setw(5)<<life<<'\n';
}
void dispVsBar(int heroMaxHP,int heroHP,int bossMaxHP,int bossHP){
    int heroBar=(float(heroHP)/heroMaxHP)*10;
    int bossBar=(float(bossHP)/bossMaxHP)*10;
    for(int i=0;i<heroBar;i++) cout<<char(178);
    for(int i=0;i<10-heroBar;i++) cout<<char(176);
    cout<<"     ";
    for(int i=0;i<10-bossBar;i++) cout<<char(176);
    for(int i=0;i<bossBar;i++) cout<<char(178);
}
void heroStat3(string heroName,int heroMaxMP,int heroMP,int heroMaxHP,int heroHP,int heroDMG,int heroDEF,int heroLIFE,int heroPath,string bossName,int bossMaxHP,int bossHP,int bossDMG,int bossDEF,int bossLIFE){
    cout<<pathName(heroPath)<<" "<<heroName<<"   vs      "<<bossName<<'\n';
    dispVsBar(heroMaxHP,heroHP,bossMaxHP,bossHP);cout<<'\n';
    cout<<rEPL(" ",5);showNum(heroHP,5);cout<<  "  HP    "<<bossHP<<'\n';
    dispVsBar(heroMaxMP,heroMP,0,0);cout<<'\n';
    cout<<rEPL(" ",5);showNum(heroMP,5);cout<<  "  MP    "<<0<<'\n';
    cout<<rEPL(" ",5);showNum(heroDMG,5);cout<< "  DMG   "<<bossDMG<<'\n';
    cout<<rEPL(" ",5);showNum(heroDEF,5);cout<< "  DEF   "<<bossDEF<<'\n';
    cout<<rEPL(" ",5);showNum(heroLIFE,5);cout<<"  LIFE  "<<bossLIFE<<'\n';
}
int cekID(int ID,int arrayID[],int size){
    for(int i=0;i<size;i++){
        if(arrayID[i]==ID) return i;
    }
    return -1;
}
bool cekEquip(int equipID,int inventoryID[]){
    bool ret=0;
    for(int i=0;i<10;i++){
        if(inventoryID[i]==equipID){
            if(equipID/10<=2) ret=1;
        }
    }
    return ret;
}
void showBag(int inventoryID[],int inventoryQTY[],int itemID[],string itemName[],int itemAttr[][3]){
    prncol(2);
    cout<<"\nMy INVENTORY"<<'\n';
    prncol(7);
    cout<<"ID NAME               ATTRIBUTE    QTY"<<'\n';
    for(int i=0;i<10;i++){
        if(inventoryID[i]==0) continue;
        prncol(2);
        cout<<inventoryID[i]<<" ";
        prncol(7);
        showStr(id2Name(inventoryID[i],itemID,itemName),15);cout<<" ";
        showAttr(inventoryID[i],itemID,itemAttr);
        cout<<"  "<<inventoryQTY[i]<<"\n";
    }
}
void showBank(int bankID[],int bankQTY[],int itemID[],string itemName[],int itemAttr[][3]){
    prncol(2);
    cout<<"\nU N I V E R S A L  B A N K"<<'\n';
    prncol(7);
    cout<<"ID NAME               ATTRIBUTE    QTY"<<'\n';
    for(int i=0;i<10;i++){
        if(bankID[i]==0) continue;
        prncol(2);
        cout<<bankID[i]<<" ";
        prncol(7);
        showStr(id2Name(bankID[i],itemID,itemName),15);cout<<" ";
        showAttr(bankID[i],itemID,itemAttr);
        cout<<"  "<<bankQTY[i]<<"\n";
    }
}
void levelUP(int gainExp,int &heroTNL, int &heroLVL, int heroPath){
    while(gainExp>0){ //big gainexp can up more than 1 lvl
        if(gainExp>=heroTNL){
            gainExp-=heroTNL;
            heroLVL++;
            heroTNL=heroLVL*100*heroPath;
        }
        else{
            heroTNL-=gainExp;
            gainExp=0;
        }
    }
}
void warningMsg(string msg,int color=7){
    prncol(color);
    for(int i=0;i<4;i++) {
        cout<<'\r';
        for(int i=0;i<msg.length();i++) cout<<' ';
        Sleep(100);
        cout<<'\r'<<msg;
        Sleep(300);
    }
    prncol(7);
    Sleep(3000);
}
void errMsg(string msg){
    prncol(4);
    cout<<msg;
    Sleep(3000);
    prncol(7);
}
int rndDrop(int itemID[],int dropRate[],int caveLvl=50){
    while(1){
        int idxID=rand()%20; //randomize pick from itemID
        if(dropRate[idxID]==0) continue; //check if non drop item then random again

        int dropChance=rand()%100;
        if(dropChance<dropRate[idxID]+caveLvl) return itemID[idxID];
        else return 0;
    }
}
void updBag(int ID,int inventoryID[],int inventoryQTY[],int qty){
    for(int i=0;i<10;i++){
        if(qty>0 and inventoryID[i]==0 and cekID(ID,inventoryID,10)==-1){
            inventoryID[i]=ID;
            inventoryQTY[i]+=qty;  //???????????
            break;
        }
        if(inventoryID[i]==ID) {
            inventoryQTY[i]+=qty;
            if(inventoryQTY[i]==0) inventoryID[i]=0;
            break;
        }
    }
}

bool vip(){ //vip passage
    cls;
    string vipcode="";
    mASKED(vipcode,0);
    if(vipcode=="batagor") return 0;
    else return 1;
}
bool isFile(string filename){
    bool ret=0;
    fstream file(filename);
    if(file) ret=1;
    file.close();
    return ret;
}
void showCatalogue(int itemID[],string itemName[],int itemCost[],int itemAttr[][3]){
    prncol(3);
    cout<<"MERCHANT CATALOGUE:"<<'\n';
    prncol(7);
    cout<<"ID NAME              PRICE     ATTRIBUTE     ID NAME              PRICE     ATTRIBUTE\n";
    for(int i=0;i<10;i++){
        //if(itemID[i]==0) continue;
        prncol(3);
        cout<<itemID[i]<<" ";
        prncol(7);
        showStr(itemName[i],15,'.');cout<<" ";
        showNum(itemCost[i],7);cout<<" ";
        for(int j=0;j<3;j++){
            showNum(itemAttr[i][j],4);
            if(j<2) cout<<'|';//99|99|99 item attr
        }
        cout<<' '<<char(179)<<' ';

        prncol(3);
        cout<<" "<<itemID[i+10]<<" ";
        prncol(7);
        showStr(itemName[i+10],15,'.');cout<<" ";
        showNum(itemCost[i+10],7);cout<<" ";
        for(int j=0;j<3;j++){
            showNum(itemAttr[i+10][j],4);
            if(j<2) cout<<'|';//99|99|99 item attr
        }
        cout<<'\n';
    }

}

void dispSkill(int heroSkill[],string skillName[],string skillDesc[]){
    cout<<"Skill:"<<'\n';
    for(int i=0;i<5;i++){
        //cout<<heroSkill[i]<<'\n';
        if(heroSkill[i]==0) continue;
        cout<<skillName[i]<<" : "<<skillDesc[i]<<'\n';
    }
}

int cekSlot(int id,int arrayId[],int size){ //cek slot available untuk simpan item
    for(int i=0;i<size;i++){
        if(arrayId[i]==id) return i;
        if(arrayId[i]==0 and cekID(id,arrayId,size)==-1) return i;
    }
    return -1;
}

